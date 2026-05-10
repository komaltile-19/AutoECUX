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

#define ECU_ID CAN_ID_BATTERY

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

    printf("[BATTERY] Sensor thread started\n");

    while (!shutdown_flag) {
        float voltage = 12.0f + ((float)rand() / RAND_MAX) * 2.0f;
        float current = -20.0f + (float)(rand() % 40);
        float soc     = 60.0f  + (float)(rand() % 35);

        pthread_mutex_lock(&sync_data.mutex);
        sync_data.data.voltage = voltage;
        sync_data.data.current = current;
        sync_data.data.soc     = soc;
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

    printf("[BATTERY] CAN thread started\n");

    while (!shutdown_flag) {
        pthread_mutex_lock(&sync_data.mutex);
        pthread_cond_wait(&sync_data.condition, &sync_data.mutex);
        local = sync_data.data;
        pthread_mutex_unlock(&sync_data.mutex);

        pack_battery(&frame, local.voltage, local.soc);

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

    printf("[BATTERY] Fault thread started\n");

    while (!shutdown_flag) {
        if (inject_fault) {
            pthread_mutex_lock(&sync_data.mutex);
            sync_data.data.voltage = 9.8f;
            sync_data.data.soc     = 5.0f;
            pthread_mutex_unlock(&sync_data.mutex);

            printf("[BATTERY] Fault injected: voltage=9.8 soc=5\n");
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

    printf("[BATTERY] Limp thread started\n");

    while (!shutdown_flag) {
        if (fd >= 0 && read_limp_command(fd, &cmd) == 0) {
            if (cmd.ecu_id == ECU_ID)
                printf("[BATTERY] Limp mode active\n");
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
    printf("[BATTERY] ECU started\n");

    signal(SIGINT,  stop_program);
    signal(SIGTERM, stop_program);
    signal(SIGUSR1, inject_signal);

    bus = can_shm_attach();
    if (!bus) {
        fprintf(stderr, "[BATTERY] Failed to attach shared memory\n");
        return EXIT_FAILURE;
    }

    if (can_sem_open() < 0) {
        fprintf(stderr, "[BATTERY] Failed to open semaphore\n");
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

    printf("[BATTERY] Shutdown complete\n");
    return 0;
}
