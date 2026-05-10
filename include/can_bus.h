#ifndef CAN_BUS_H
#define CAN_BUS_H

#include <stdint.h>
#include <time.h>

/* CAN IDs */
#define CAN_ID_ENGINE   100
#define CAN_ID_ABS      101
#define CAN_ID_TRANS    102
#define CAN_ID_BATTERY  103

/* Ring buffer depth */
#ifndef CAN_BUS_SLOTS
#define CAN_BUS_SLOTS  64
#endif

/* -----------------------------------------------------------------------
   CAN frame
   ----------------------------------------------------------------------- */
typedef struct {
    uint32_t can_id;
    uint8_t  data[8];
    uint8_t  dlc;
    uint64_t timestamp;
    uint8_t  valid;
} can_frame_t;

/* -----------------------------------------------------------------------
   CAN bus ring buffer (lives in shared memory)
   ----------------------------------------------------------------------- */
typedef struct {
    can_frame_t frames[CAN_BUS_SLOTS];
    int head;
    int tail;
} can_bus_t;

/* -----------------------------------------------------------------------
   Monotonic microsecond clock
   ----------------------------------------------------------------------- */
static inline uint64_t get_time_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/* Alias used by fault_detector.c */
static inline uint64_t can_time_us(void) { return get_time_us(); }

/* -----------------------------------------------------------------------
   Shared memory API
   ----------------------------------------------------------------------- */
can_bus_t *can_shm_create(void);
can_bus_t *can_shm_attach(void);
void       can_shm_detach(can_bus_t *bus);
void       can_shm_destroy(void);

int can_write(can_bus_t *bus, can_frame_t *frame);
int can_read (can_bus_t *bus, can_frame_t *frame, int *tail);

/* -----------------------------------------------------------------------
   Pack helpers — encode sensor values into a CAN frame
   ----------------------------------------------------------------------- */
void pack_engine     (can_frame_t *f, float rpm, float temp, float throttle);
void pack_abs        (can_frame_t *f, float s1, float s2, float s3, float s4, float slip);
void pack_transmission(can_frame_t *f, int gear, float torque);
void pack_battery    (can_frame_t *f, float voltage, float soc);

/* -----------------------------------------------------------------------
   Unpack helpers — decode sensor values from a CAN frame
   ----------------------------------------------------------------------- */
float get_engine_rpm    (const can_frame_t *f);
float get_engine_temp   (const can_frame_t *f);
float get_abs_slip      (const can_frame_t *f);
int   get_trans_gear    (const can_frame_t *f);
float get_battery_voltage(const can_frame_t *f);

#endif
