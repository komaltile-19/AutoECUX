#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../include/config.h"
#include "../include/ecu_semaphore.h"

static sem_t *can_sem = NULL;

int can_sem_create(void)
{
    sem_unlink(SEM_NAME);
    can_sem = sem_open(SEM_NAME, O_CREAT, 0666, 1);
    if (can_sem == SEM_FAILED) {
        perror("sem_open create");
        return -1;
    }
    printf("[can_sem] Created %s\n", SEM_NAME);
    return 0;
}

int can_sem_open(void)
{
    can_sem = sem_open(SEM_NAME, 0);
    if (can_sem == SEM_FAILED) {
        perror("sem_open attach");
        return -1;
    }
    return 0;
}

void can_sem_acquire(void)
{
    sem_wait(can_sem);
}

void can_sem_release(void)
{
    sem_post(can_sem);
}

void can_sem_close(void)
{
    if (can_sem)
        sem_close(can_sem);
}

void can_sem_destroy(void)
{
    can_sem_close();
    sem_unlink(SEM_NAME);
    printf("[can_sem] Removed %s\n", SEM_NAME);
}

void print_semaphore_value(void)
{
    int value = 0;
    if (can_sem)
        sem_getvalue(can_sem, &value);
    printf("[can_sem] value=%d\n", value);
}
