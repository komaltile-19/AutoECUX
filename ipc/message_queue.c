#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include "../include/dtc.h"
#include "../include/config.h"

static int dtc_fd = -1;

/* -----------------------------------------------------------------------
   Create the DTC named FIFO (called by gateway at startup)
   ----------------------------------------------------------------------- */
int create_dtc_fifo(void)
{
    unlink(DTC_PIPE);

    if (mkfifo(DTC_PIPE, 0666) < 0) {
        perror("mkfifo DTC_PIPE");
        return -1;
    }

    /* Open read-write so the gateway fd stays alive even when no reader */
    dtc_fd = open(DTC_PIPE, O_RDWR);
    if (dtc_fd < 0) {
        perror("open DTC_PIPE");
        return -1;
    }

    printf("[dtc_mq] Created %s\n", DTC_PIPE);
    return 0;
}

/* -----------------------------------------------------------------------
   Open for writing — used by fault_detector
   ----------------------------------------------------------------------- */
int dtc_mq_open_write(void)
{
    dtc_fd = open(DTC_PIPE, O_WRONLY);
    if (dtc_fd < 0) {
        perror("dtc_mq_open_write");
        return -1;
    }
    return dtc_fd;
}

/* Alias kept for legacy callers */
int open_dtc_write(void) { return dtc_mq_open_write(); }

/* -----------------------------------------------------------------------
   Open for reading — used by alert_manager
   ----------------------------------------------------------------------- */
int open_dtc_read(void)
{
    dtc_fd = open(DTC_PIPE, O_RDONLY);
    if (dtc_fd < 0) {
        perror("open_dtc_read");
        return -1;
    }
    return dtc_fd;
}

/* -----------------------------------------------------------------------
   Return the current file descriptor
   ----------------------------------------------------------------------- */
int dtc_mq_fd(void)
{
    return dtc_fd;
}

int get_dtc_fd(void) { return dtc_fd; }

/* -----------------------------------------------------------------------
   Write one DTC message into the pipe
   ----------------------------------------------------------------------- */
int dtc_send(int fd, dtc_msg_t *msg)
{
    ssize_t n = write(fd, msg, sizeof(dtc_msg_t));
    if (n != (ssize_t)sizeof(dtc_msg_t)) {
        perror("dtc_send write");
        return -1;
    }
    return 0;
}

/* Alias */
int send_dtc(int fd, dtc_msg_t *msg) { return dtc_send(fd, msg); }

/* -----------------------------------------------------------------------
   Read one DTC message from the pipe
   ----------------------------------------------------------------------- */
int receive_dtc(int fd, dtc_msg_t *msg)
{
    ssize_t n = read(fd, msg, sizeof(dtc_msg_t));
    if (n != (ssize_t)sizeof(dtc_msg_t)) {
        return -1;
    }
    return 0;
}

/* -----------------------------------------------------------------------
   Close
   ----------------------------------------------------------------------- */
void dtc_mq_close(void)
{
    if (dtc_fd >= 0) {
        close(dtc_fd);
        dtc_fd = -1;
    }
}

void close_dtc(void) { dtc_mq_close(); }

/* -----------------------------------------------------------------------
   Remove the FIFO from the filesystem (called by gateway on shutdown)
   ----------------------------------------------------------------------- */
void destroy_dtc_fifo(void)
{
    dtc_mq_close();
    unlink(DTC_PIPE);
    printf("[dtc_mq] Removed %s\n", DTC_PIPE);
}
