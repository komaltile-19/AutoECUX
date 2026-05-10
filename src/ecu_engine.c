#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>

#include "../include/config.h"
#include "../include/can_bus.h"
#include "../include/sensor.h"
#include "../include/ecu_semaphore.h"

#define ECU_ID CAN_ID_ENGINE

static volatile sig_atomic_t running = 1;
static volatile sig_atomic_t inject_fault = 0;

static ecu_sync_t sync_data;
static can_bus_t *bus = NULL;

static void stop_program(int sig)  { (void)sig; running = 0; }
static void inject_signal(int sig) { (void)sig; inject_fault = 1; }

/* -----------------------------------------------------------------------
   Sensor thread: generate engine data and wake the CAN thread
   ----------------------------------------------------------------------- */
void *sensor_thread(void *arg)
{
    (void)arg;
    srand((unsigned)time(NULL));

    while (running) {
        float rpm      = 800.0f  + (float)(rand() % 5700);
        float temp     = 70.0f   + (float)(rand() % 30);
        float throttle = (float)(rand() % 101);

        pthread_mutex_lock(&sync_data.mutex);

        sync_data.data.rpm      = rpm;
        sync_data.data.temp     = temp;
        sync_data.data.throttle = throttle;

        pthread_cond_signal(&sync_data.condition);
        pthread_mutex_unlock(&sync_data.mutex);

        usleep(50000);
    }
    return NULL;
}

/* -----------------------------------------------------------------------
   CAN writer thread: pack data and push to shared memory ring buffer
   ----------------------------------------------------------------------- */
void *can_thread(void *arg)
{
    (void)arg;

    sensor_data_t local;
    can_frame_t   frame;

    while (running) {
        pthread_mutex_lock(&sync_data.mutex);
        pthread_cond_wait(&sync_data.condition, &sync_data.mutex);
        local = sync_data.data;
        pthread_mutex_unlock(&sync_data.mutex);

        pack_engine(&frame, local.rpm, local.temp, local.throttle);

        can_sem_acquire();
        can_write(bus, &frame);
        can_sem_release();
    }
    return NULL;
}

/* -----------------------------------------------------------------------
   Fault monitor thread: logs faults periodically
   ----------------------------------------------------------------------- */
void *fault_thread(void *arg)
{
    (void)arg;

    while (running) {
        pthread_mutex_lock(&sync_data.mutex);
        float rpm  = sync_data.data.rpm;
        float temp = sync_data.data.temp;
        pthread_mutex_unlock(&sync_data.mutex);

        if (rpm > HIGH_RPM)
            printf("[ENGINE] Warning: high RPM=%.0f\n", rpm);
        if (temp > HIGH_TEMP)
            printf("[ENGINE] Warning: high temp=%.0f\n", temp);

        sleep(2);
    }
    return NULL;
}

/* -----------------------------------------------------------------------
   Limp mode thread: listens for fault injection signal
   ----------------------------------------------------------------------- */
void *limp_thread(void *arg)
{
    (void)arg;

    while (running) {
        if (inject_fault) {
            pthread_mutex_lock(&sync_data.mutex);
            sync_data.data.rpm  = 9500.0f;
            sync_data.data.temp = 120.0f;
            pthread_mutex_unlock(&sync_data.mutex);

            printf("[ENGINE] Fault injected: RPM=9500 temp=120\n");
            inject_fault = 0;
        }
        usleep(100000);
    }
    return NULL;
}

/* -----------------------------------------------------------------------
   Main
   ----------------------------------------------------------------------- */
int main(void)
{
    printf("[ENGINE] ECU Engine started\n");

    signal(SIGINT,  stop_program);
    signal(SIGTERM, stop_program);
    signal(SIGUSR1, inject_signal);

    bus = can_shm_attach();
    if (!bus) {
        fprintf(stderr, "[ENGINE] Failed to attach shared memory\n");
        return EXIT_FAILURE;
    }

    if (can_sem_open() < 0) {
        fprintf(stderr, "[ENGINE] Failed to open semaphore\n");
        return EXIT_FAILURE;
    }

    pthread_mutex_init(&sync_data.mutex, NULL);
    pthread_cond_init(&sync_data.condition, NULL);

    pthread_t t1, t2, t3, t4;
    pthread_create(&t1, NULL, sensor_thread, NULL);
    pthread_create(&t2, NULL, can_thread,    NULL);
    pthread_create(&t3, NULL, fault_thread,  NULL);
    pthread_create(&t4, NULL, limp_thread,   NULL);

    while (running)
        sleep(1);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_join(t3, NULL);
    pthread_join(t4, NULL);

    pthread_mutex_destroy(&sync_data.mutex);
    pthread_cond_destroy(&sync_data.condition);

    can_shm_detach(bus);
    can_sem_close();

    printf("[ENGINE] Shutdown complete\n");
    return 0;
}
