#ifndef FIFO_COMM_H
#define FIFO_COMM_H

#include "../include/sensor.h"

int  create_limp_fifo    (void);
int  open_fifo_write     (void);
int  open_fifo_read      (void);
int  write_limp_command  (int fd, limp_cmd_t *cmd);
int  read_limp_command   (int fd, limp_cmd_t *cmd);
void close_fifo          (int fd);
void destroy_fifo        (void);

#endif
