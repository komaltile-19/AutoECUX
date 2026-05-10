#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>

#include "../include/config.h"
#include "../include/can_bus.h"
#include "../include/sensor.h"
#include "../include/ecu_semaphore.h"
#include "../ipc/fifo_comm.h"

#define ECU_ID CAN_ID_ABS

static volatile sig_atomic_t shutdown_flag = 0;
static volatile sig_atomic_t inject_fault  = 0;

static ecu_sync_t sync_data;
static can_bus_t *bus = NULL;

static void stop_program(int sig)  { (void)sig; shutdown_flag = 1; }
static void inject_signal(int sig) { (void)sig; inject_fault  = 1; }

/* -----------------------------------------------------------------------
   Sensor thread
   ----------------------------------------------------------------------- */
void *sensor_thread(void *arg)
{
    (void)arg;
    srand((unsigned)time(NULL));

    while (!shutdown_flag) {
        float speed = 30.0f + (float)(rand() % 100);
        float slip  = 0.01f + ((float)rand() / RAND_MAX) * 0.05f;

        pthread_mutex_lock(&sync_data.mutex);
        for (int i = 0; i < 4; i++)
            sync_data.data.wheel_speed[i] = speed;
        sync_data.data.slip = slip;
        pthread_cond_signal(&sync_data.condition);
        pthread_mutex_unlock(&sync_data.mutex);

        usleep(50000);
    }
    return NULL;
}

/* -----------------------------------------------------------------------
   CAN writer thread
   ----------------------------------------------------------------------- */
void *can_thread(void *arg)
{
    (void)arg;

    sensor_data_t local;
    can_frame_t   frame;

    while (!shutdown_flag) {
        pthread_mutex_lock(&sync_data.mutex);
        pthread_cond_wait(&sync_data.condition, &sync_data.mutex);
        local = sync_data.data;
        pthread_mutex_unlock(&sync_data.mutex);

        pack_abs(&frame,
                 local.wheel_speed[0],
                 local.wheel_speed[1],
                 local.wheel_speed[2],
                 local.wheel_speed[3],
                 local.slip);

        can_sem_acquire();
        can_write(bus, &frame);
        can_sem_release();
    }
    return NULL;
}

/* -----------------------------------------------------------------------
   Fault injection thread
   ----------------------------------------------------------------------- */
void *fault_thread(void *arg)
{
    (void)arg;

    while (!shutdown_flag) {
        if (inject_fault) {
            pthread_mutex_lock(&sync_data.mutex);
            sync_data.data.wheel_speed[0] = 0.0f;
            sync_data.data.slip           = 0.95f;
            pthread_mutex_unlock(&sync_data.mutex);

            printf("[ABS] Fault injected: wheel_speed[0]=0 slip=0.95\n");
            inject_fault = 0;
        }
        usleep(100000);
    }
    return NULL;
}

/* -----------------------------------------------------------------------
   Limp mode thread
   ----------------------------------------------------------------------- */
void *limp_thread(void *arg)
{
    (void)arg;

    limp_cmd_t cmd;
    int fd = open_fifo_read();

    while (!shutdown_flag) {
        if (fd >= 0 && read_limp_command(fd, &cmd) == 0) {
            if (cmd.ecu_id == ECU_ID)
                printf("[ABS] Limp mode activated: limit=%.0f\n", cmd.limit);
        }
        usleep(100000);
    }

    close_fifo(fd);
    return NULL;
}

/* -----------------------------------------------------------------------
   Main
   ----------------------------------------------------------------------- */
int main(void)
{
    printf("[ABS] ECU started\n");

    signal(SIGINT,  stop_program);
    signal(SIGTERM, stop_program);
    signal(SIGUSR1, inject_signal);

    bus = can_shm_attach();
    if (!bus) {
        fprintf(stderr, "[ABS] Failed to attach shared memory\n");
        return EXIT_FAILURE;
    }

    if (can_sem_open() < 0) {
        fprintf(stderr, "[ABS] Failed to open semaphore\n");
        return EXIT_FAILURE;
    }

    pthread_mutex_init(&sync_data.mutex, NULL);
    pthread_cond_init(&sync_data.condition, NULL);

    pthread_t t1, t2, t3, t4;
    pthread_create(&t1, NULL, sensor_thread, NULL);
    pthread_create(&t2, NULL, can_thread,    NULL);
    pthread_create(&t3, NULL, fault_thread,  NULL);
    pthread_create(&t4, NULL, limp_thread,   NULL);

    while (!shutdown_flag)
        usleep(50000);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_join(t3, NULL);
    pthread_join(t4, NULL);

    pthread_mutex_destroy(&sync_data.mutex);
    pthread_cond_destroy(&sync_data.condition);

    can_shm_detach(bus);
    can_sem_close();

    printf("[ABS] Shutdown complete\n");
    return 0;
}
