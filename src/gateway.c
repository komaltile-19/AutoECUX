#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>

#include "../include/config.h"
#include "../include/can_bus.h"
#include "../include/sensor.h"
#include "../include/dtc.h"

#include "../ipc/can_shared_memory.h"
#include "../ipc/semaphore.h"
#include "../ipc/message_queue.h"
#include "../ipc/fifo_comm.h"

static volatile sig_atomic_t g_shutdown = 0;
static volatile sig_atomic_t g_alarm    = 0;
static volatile sig_atomic_t g_child    = 0;
static volatile sig_atomic_t g_dtc      = 0;

static pid_t children[16];
static int   child_count = 0;

static int pipefd[2];

static int total_dtcs = 0;
static int snap_index = 0;

/* -----------------------------------------------------------------------
   Signal handlers
   ----------------------------------------------------------------------- */
static void h_shutdown(int sig) { (void)sig; g_shutdown = 1; }
static void h_alarm   (int sig) { (void)sig; g_alarm    = 1; }
static void h_child   (int sig) { (void)sig; g_child    = 1; }
static void h_dtc     (int sig) { (void)sig; g_dtc      = 1; }

static void install_handler(int sig, void (*handler)(int))
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags   = SA_RESTART;
    sigaction(sig, &sa, NULL);
}

/* -----------------------------------------------------------------------
   Read thresholds.conf at startup
   ----------------------------------------------------------------------- */
static void read_config_file(void)
{
    int fd = open(CONFIG_THRESHOLD_FILE, O_RDONLY);
    if (fd < 0) {
        printf("[gateway] Config file not found. Using defaults.\n");
        return;
    }

    char buf[512];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        printf("[gateway] Config loaded (%zd bytes) from %s\n",
               n, CONFIG_THRESHOLD_FILE);
    }
    close(fd);
}

/* -----------------------------------------------------------------------
   Fork + execv an ECU binary
   ----------------------------------------------------------------------- */
static void spawn_process(const char *path)
{
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return; }

    if (pid == 0) {
        close(pipefd[1]);
        char *argv[] = { (char *)path, NULL };
        execv(path, argv);
        perror("execv");
        exit(EXIT_FAILURE);
    }

    children[child_count++] = pid;
    printf("[gateway] Started %s PID=%d\n", path, pid);
}

/* -----------------------------------------------------------------------
   Fork the OBD logger (passes the read end of the anonymous pipe)
   ----------------------------------------------------------------------- */
static void spawn_logger(void)
{
    pid_t pid = fork();
    if (pid < 0) { perror("fork logger"); return; }

    if (pid == 0) {
        close(pipefd[1]);
        char fd_str[16];
        snprintf(fd_str, sizeof(fd_str), "%d", pipefd[0]);
        char *argv[] = { "./build/obd_logger", fd_str, NULL };
        execv("./build/obd_logger", argv);
        perror("execv obd_logger");
        exit(EXIT_FAILURE);
    }

    children[child_count++] = pid;
    close(pipefd[0]);
    printf("[gateway] Started obd_logger PID=%d\n", pid);
}

/* -----------------------------------------------------------------------
   Periodic snapshot (every 5 s via SIGALRM)
   ----------------------------------------------------------------------- */
static void send_snapshot(void)
{
    char msg[64];
    int  n = snprintf(msg, sizeof(msg), "SNAP:%d dtcs=%d\n",
                      snap_index++, total_dtcs);
    if (write(pipefd[1], msg, n) < 0) { /* ignore */ }
    alarm(5);
    printf("[gateway] Snapshot sent snap=%d dtcs=%d\n",
           snap_index - 1, total_dtcs);
}

/* -----------------------------------------------------------------------
   Forward DTC notification to obd_logger via the pipe
   ----------------------------------------------------------------------- */
static void send_dtc_message(void)
{
    total_dtcs++;
    char msg[64];
    int  n = snprintf(msg, sizeof(msg), "DTC:FAULT-%d\n", total_dtcs);
    if (write(pipefd[1], msg, n) < 0) { /* ignore */ }
    printf("[gateway] DTC forwarded total=%d\n", total_dtcs);
}

/* -----------------------------------------------------------------------
   Reap any zombie children
   ----------------------------------------------------------------------- */
static void handle_children(void)
{
    int   status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
        printf("[gateway] Child exited PID=%d\n", pid);
}

/* -----------------------------------------------------------------------
   Graceful shutdown: SIGTERM all children, then clean up IPC
   ----------------------------------------------------------------------- */
static void shutdown_system(void)
{
    printf("[gateway] Shutting down — sending SIGTERM to %d children\n",
           child_count);

    for (int i = 0; i < child_count; i++)
        kill(children[i], SIGTERM);

    for (int i = 0; i < child_count; i++)
        waitpid(children[i], NULL, 0);

    char msg[64];
    int  n = snprintf(msg, sizeof(msg), "REPORT:%d\n", total_dtcs);
    if (write(pipefd[1], msg, n) < 0) { /* ignore */ }

    close(pipefd[1]);

    can_shm_destroy();
    can_sem_destroy();
    destroy_dtc_fifo();
    destroy_fifo();

    printf("[gateway] IPC cleaned\n");
}

/* -----------------------------------------------------------------------
   Main
   ----------------------------------------------------------------------- */
int main(void)
{
    printf("[gateway] Automotive ECU Simulator started PID=%d\n", getpid());

    mkdir("build", 0755);
    mkdir("logs",  0755);

    read_config_file();

    /* Create all IPC resources before spawning children */
    if (!can_shm_create())       { perror("can_shm_create"); return EXIT_FAILURE; }
    if (can_sem_create()   < 0)  { perror("can_sem_create"); return EXIT_FAILURE; }
    if (create_dtc_fifo()  < 0)  { perror("create_dtc_fifo"); return EXIT_FAILURE; }
    if (create_limp_fifo() < 0)  { perror("create_limp_fifo"); return EXIT_FAILURE; }

    if (pipe(pipefd) < 0) { perror("pipe"); return EXIT_FAILURE; }

    install_handler(SIGINT,  h_shutdown);
    install_handler(SIGTERM, h_shutdown);
    install_handler(SIGALRM, h_alarm);
    install_handler(SIGCHLD, h_child);
    install_handler(SIGUSR2, h_dtc);

    /* Start logger first so it is ready when ECUs produce data */
    spawn_logger();
    sleep(1);

    spawn_process("./build/ecu_engine");
    spawn_process("./build/ecu_abs");
    spawn_process("./build/ecu_trans");
    spawn_process("./build/ecu_battery");
    spawn_process("./build/fault_detector");
    spawn_process("./build/alert_manager");

    printf("[gateway] All processes started. Press Ctrl-C to stop.\n");

    alarm(5);

    while (!g_shutdown) {
        if (g_alarm) { g_alarm = 0; send_snapshot();    }
        if (g_dtc)   { g_dtc   = 0; send_dtc_message(); }
        if (g_child) { g_child = 0; handle_children();  }
        usleep(100000);
    }

    shutdown_system();
    printf("[gateway] Clean exit\n");
    return 0;
}
