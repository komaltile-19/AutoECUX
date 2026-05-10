#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#include "../include/config.h"
#include "../include/dtc.h"
#include "../include/sensor.h"
#include "../ipc/message_queue.h"
#include "../ipc/fifo_comm.h"

#define MAX_DTC 50

static volatile sig_atomic_t shutdown_flag = 0;

static dtc_msg_t dtc_list[MAX_DTC];
static int dtc_count = 0;

/* optional global visibility (if gateway uses it) */
int global_dtc_count = 0;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  cond  = PTHREAD_COND_INITIALIZER;

static int dtc_fd  = -1;
static int limp_fd = -1;

static void stop_program(int sig)
{
    (void)sig;
    shutdown_flag = 1;
    pthread_cond_broadcast(&cond);
}

/* ---------------- THREAD 1: READ DTC ---------------- */
void *read_dtc_thread(void *arg)
{
    (void)arg;
    dtc_msg_t msg;

    printf("[alert_manager] DTC reader thread started\n");

    while (!shutdown_flag) {

        if (receive_dtc(dtc_fd, &msg) == 0) {

            printf("[alert_manager] Received DTC: %s (%s)\n",
                   msg.code, get_severity(msg.severity));

            pthread_mutex_lock(&mutex);

            if (dtc_count < MAX_DTC) {
                dtc_list[dtc_count++] = msg;

                /* 🔥 FIX: proper DTC counter increment */
                global_dtc_count++;

                pthread_cond_signal(&cond);
            }

            pthread_mutex_unlock(&mutex);
        }

        usleep(10000);
    }

    return NULL;
}

/* ---------------- THREAD 2: PROCESS DTC ---------------- */
void *process_dtc_thread(void *arg)
{
    (void)arg;

    dtc_msg_t  msg;
    limp_cmd_t cmd;

    printf("[alert_manager] DTC processing thread started\n");

    while (!shutdown_flag) {

        pthread_mutex_lock(&mutex);

        while (dtc_count == 0 && !shutdown_flag)
            pthread_cond_wait(&cond, &mutex);

        if (shutdown_flag) {
            pthread_mutex_unlock(&mutex);
            break;
        }

        msg = dtc_list[--dtc_count];

        pthread_mutex_unlock(&mutex);

        printf("[alert_manager] Processing DTC: %s severity=%s\n",
               msg.code, get_severity(msg.severity));

        /* critical fault → limp mode */
        if (msg.severity == CRITICAL) {

            cmd.ecu_id  = msg.ecu_id;
            cmd.command = 1;
            cmd.limit   = RPM_LIMIT;

            write_limp_command(limp_fd, &cmd);

            printf("[alert_manager] LIMP MODE activated for ECU %u\n",
                   msg.ecu_id);
        }
        else {
            printf("[alert_manager] Warning logged\n");
        }
    }

    return NULL;
}

/* ---------------- MAIN ---------------- */
int main(void)
{
    printf("[alert_manager] Started\n");

    signal(SIGINT, stop_program);
    signal(SIGTERM, stop_program);

    dtc_fd  = open_dtc_read();
    limp_fd = open_fifo_write();

    if (dtc_fd < 0 || limp_fd < 0) {
        fprintf(stderr, "[alert_manager] FIFO open failed\n");
        return EXIT_FAILURE;
    }

    pthread_t t1, t2;

    pthread_create(&t1, NULL, read_dtc_thread, NULL);
    pthread_create(&t2, NULL, process_dtc_thread, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    close_dtc();
    close_fifo(limp_fd);

    printf("[alert_manager] Stopped. Total DTCs=%d\n", global_dtc_count);

    return 0;
}
