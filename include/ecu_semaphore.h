#ifndef SEMAPHORE_H
#define SEMAPHORE_H

int  can_sem_create (void);
int  can_sem_open   (void);
void can_sem_acquire(void);
void can_sem_release(void);
void can_sem_close  (void);
void can_sem_destroy(void);
void print_semaphore_value(void);

#endif
