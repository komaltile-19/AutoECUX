#ifndef SENSOR_H
#define SENSOR_H

#include <stdint.h>
#include <pthread.h>
#include "config.h"

/* -----------------------------------------------------------------------
   All sensor readings for every ECU in one flat struct.
   Each ECU only fills in the fields it owns.
   ----------------------------------------------------------------------- */
typedef struct {
    /* Engine */
    float rpm;
    float temp;
    float throttle;

    /* ABS */
    float wheel_speed[4];
    float slip;

    /* Transmission */
    int   gear;
    float torque;

    /* Battery */
    float voltage;
    float current;
    float soc;
} sensor_data_t;

/* -----------------------------------------------------------------------
   Runtime thresholds (populated from thresholds.conf)
   ----------------------------------------------------------------------- */
typedef struct {
    float max_rpm;
    float max_temp;
    float max_speed;
    float max_slip;
    float min_voltage;
    float max_voltage;
} threshold_t;

/* -----------------------------------------------------------------------
   Limp-home command sent over LIMP_PIPE
   ----------------------------------------------------------------------- */
typedef struct {
    uint32_t ecu_id;
    uint8_t  command;
    float    limit;
} limp_cmd_t;

/* -----------------------------------------------------------------------
   Shared data between sensor thread and CAN writer thread inside one ECU
   ----------------------------------------------------------------------- */
typedef struct {
    sensor_data_t  data;
    pthread_mutex_t mutex;
    pthread_cond_t  condition;
    int             inject_fault;
    int             shutdown;
} ecu_sync_t;

/* -----------------------------------------------------------------------
   Snapshot record written to logs/snapshot.bin
   ----------------------------------------------------------------------- */
typedef struct {
    int      record_no;
    uint64_t time;
    float    rpm;
    float    voltage;
} snapshot_t;

typedef struct {
    int total_records;
    int magic_number;
} snap_header_t;

#endif
