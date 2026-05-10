#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#include "../include/config.h"
#include "../include/can_bus.h"
#include "../include/sensor.h"
#include "../include/dtc.h"
#include "../include/ecu_semaphore.h"
#include "../ipc/message_queue.h"

static volatile sig_atomic_t stop_flag = 0;

static can_bus_t *bus = NULL;
static int tail = 0;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  cond  = PTHREAD_COND_INITIALIZER;

static int fault_found = 0;
static uint32_t fault_ecu = 0;

static void stop_program(int sig) {
    (void)sig;
    stop_flag = 1;
    pthread_cond_broadcast(&cond);
}

/* ---------------- SCAN THREAD ---------------- */
void *scan_thread(void *arg)
{
    (void)arg;
    can_frame_t frame;

    printf("[fault_detector] Scan thread started\n");

    while (!stop_flag) {
        can_sem_acquire();

        while (can_read(bus, &frame, &tail)) {

            uint32_t id = frame.can_id;
            int fault = 0;

            if (id == CAN_ID_ENGINE) {
                float rpm = get_engine_rpm(&frame);
                if (rpm > HIGH_RPM) fault = 1;
            }
            else if (id == CAN_ID_ABS) {
                float slip = get_abs_slip(&frame);
                if (slip > 0.80f) fault = 1;
            }
            else if (id == CAN_ID_TRANS) {
                int gear = get_trans_gear(&frame);
                if (gear == 0) fault = 1;
            }
            else if (id == CAN_ID_BATTERY) {
                float v = get_battery_voltage(&frame);
                if (v < LOW_VOLTAGE) fault = 1;
            }

            if (fault) {
                pthread_mutex_lock(&mutex);
                fault_ecu = id;
                fault_found = 1;
                pthread_cond_signal(&cond);
                pthread_mutex_unlock(&mutex);
            }
        }

        can_sem_release();
        usleep(50000);
    }

    return NULL;
}

/* ---------------- CLASSIFIER THREAD ---------------- */
void *classifier_thread(void *arg)
{
    (void)arg;
    dtc_msg_t dtc;

    printf("[fault_detector] Classifier thread started\n");

    while (!stop_flag) {

        pthread_mutex_lock(&mutex);
        while (!fault_found && !stop_flag)
            pthread_cond_wait(&cond, &mutex);

        if (stop_flag) {
            pthread_mutex_unlock(&mutex);
            break;
        }

        uint32_t ecu = fault_ecu;
        fault_found = 0;
        pthread_mutex_unlock(&mutex);

        memset(&dtc, 0, sizeof(dtc));

        dtc.ecu_id = ecu;
        dtc.severity = CRITICAL;
        dtc.timestamp_us = can_time_us();

        dtc_entry_t *entry = find_dtc(ecu);

        if (entry) {
            strncpy(dtc.code, entry->code, sizeof(dtc.code)-1);
            strncpy(dtc.description, entry->description, sizeof(dtc.description)-1);
            dtc.severity = entry->severity;
        } else {
            snprintf(dtc.code, sizeof(dtc.code), "U%04X", ecu);
            snprintf(dtc.description, sizeof(dtc.description),
                     "Unknown ECU 0x%X", ecu);
        }

        printf("[fault_detector] DTC GENERATED: %s\n", dtc.code);

        dtc_send(dtc_mq_fd(), &dtc);
    }

    return NULL;
}

/* ---------------- MAIN ---------------- */
int main(void)
{
    printf("[fault_detector] Started\n");

    signal(SIGINT, stop_program);
    signal(SIGTERM, stop_program);

    bus = can_shm_attach();
    if (!bus) return EXIT_FAILURE;

    if (can_sem_open() < 0) return EXIT_FAILURE;
    if (dtc_mq_open_write() < 0) return EXIT_FAILURE;

    pthread_t t1, t2;
    pthread_create(&t1, NULL, scan_thread, NULL);
    pthread_create(&t2, NULL, classifier_thread, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    dtc_mq_close();
    can_shm_detach(bus);
    can_sem_close();

    return 0;
}
