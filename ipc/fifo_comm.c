#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include "../include/sensor.h"
#include "../include/config.h"

/* -----------------------------------------------------------------------
   Create the limp-home FIFO (called by gateway)
   ----------------------------------------------------------------------- */
int create_limp_fifo(void)
{
    unlink(LIMP_PIPE);

    if (mkfifo(LIMP_PIPE, 0666) < 0) {
        perror("mkfifo LIMP_PIPE");
        return -1;
    }

    printf("[limp_fifo] Created %s\n", LIMP_PIPE);
    return 0;
}

/* -----------------------------------------------------------------------
   Open for writing — used by alert_manager to send limp commands
   ----------------------------------------------------------------------- */
int open_fifo_write(void)
{
    int fd = open(LIMP_PIPE, O_WRONLY);
    if (fd < 0) {
        perror("open_fifo_write");
        return -1;
    }
    return fd;
}

/* -----------------------------------------------------------------------
   Open for reading — used by ECU limp threads
   ----------------------------------------------------------------------- */
int open_fifo_read(void)
{
    int fd = open(LIMP_PIPE, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        perror("open_fifo_read");
        return -1;
    }
    return fd;
}

/* -----------------------------------------------------------------------
   Write one limp command
   ----------------------------------------------------------------------- */
int write_limp_command(int fd, limp_cmd_t *cmd)
{
    ssize_t n = write(fd, cmd, sizeof(limp_cmd_t));
    if (n != (ssize_t)sizeof(limp_cmd_t)) {
        perror("write_limp_command");
        return -1;
    }
    return 0;
}

/* -----------------------------------------------------------------------
   Read one limp command (non-blocking; returns -1 if nothing ready)
   ----------------------------------------------------------------------- */
int read_limp_command(int fd, limp_cmd_t *cmd)
{
    ssize_t n = read(fd, cmd, sizeof(limp_cmd_t));
    if (n != (ssize_t)sizeof(limp_cmd_t)) {
        return -1;
    }
    return 0;
}

/* -----------------------------------------------------------------------
   Close a FIFO file descriptor
   ----------------------------------------------------------------------- */
void close_fifo(int fd)
{
    if (fd >= 0)
        close(fd);
}

/* -----------------------------------------------------------------------
   Remove the limp FIFO from the filesystem
   ----------------------------------------------------------------------- */
void destroy_fifo(void)
{
    unlink(LIMP_PIPE);
    printf("[limp_fifo] Removed %s\n", LIMP_PIPE);
}
