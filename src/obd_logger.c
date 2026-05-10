#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <time.h>

#define DTC_LOG "logs/dtc.log"

static volatile sig_atomic_t shutdown_flag = 0;

static void handler(int sig)
{
    (void)sig;
    shutdown_flag = 1;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: obd_logger <pipe_fd>\n");
        return EXIT_FAILURE;
    }

    int pipe_fd = atoi(argv[1]);

    signal(SIGINT, handler);
    signal(SIGTERM, handler);

    mkdir("logs", 0755);

    int fd_dtc = open(DTC_LOG, O_WRONLY | O_CREAT | O_APPEND, 0644);

    if (fd_dtc < 0) {
        perror("[obd_logger] open log file");
        return EXIT_FAILURE;
    }

    char line[128];
    int pos = 0;

    printf("[obd_logger] started, reading pipe fd=%d\n", pipe_fd);

    while (!shutdown_flag) {

        char ch;
        ssize_t n = read(pipe_fd, &ch, 1);

        if (n <= 0)
            break;

        if (ch == '\n') {

            line[pos] = '\0';
            pos = 0;

            /* ---------------- DTC MESSAGE ---------------- */
            if (strncmp(line, "DTC:", 4) == 0) {

                char msg[256];
                time_t t = time(NULL);

                snprintf(msg, sizeof(msg),
                         "[%ld] %s\n", (long)t, line);

                write(fd_dtc, msg, strlen(msg));

                printf("[obd_logger] FAULT DETECTED → %s\n", line);
            }

            /* ---------------- SNAPSHOT ---------------- */
            else if (strncmp(line, "SNAP:", 5) == 0) {
                printf("[obd_logger] %s\n", line);
            }

        } else {
            if (pos < (int)sizeof(line) - 1)
                line[pos++] = ch;
        }
    }

    close(fd_dtc);
    close(pipe_fd);

    printf("[obd_logger] stopped\n");
    return 0;
}
