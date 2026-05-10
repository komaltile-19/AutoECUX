#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#include "../include/dtc.h"

int  create_dtc_fifo   (void);
int  dtc_mq_open_write (void);
int  open_dtc_read     (void);
int  dtc_mq_fd         (void);
int  dtc_send          (int fd, dtc_msg_t *msg);
int  send_dtc          (int fd, dtc_msg_t *msg);
int  receive_dtc       (int fd, dtc_msg_t *msg);
void dtc_mq_close      (void);
void close_dtc         (void);
void destroy_dtc_fifo  (void);

#endif
