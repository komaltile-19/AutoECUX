#ifndef CAN_SHARED_MEMORY_H
#define CAN_SHARED_MEMORY_H

#include "../include/can_bus.h"

can_bus_t *can_shm_create(void);
can_bus_t *can_shm_attach(void);
void       can_shm_detach(can_bus_t *bus);
void       can_shm_destroy(void);

int can_write(can_bus_t *bus, can_frame_t *frame);
int can_read (can_bus_t *bus, can_frame_t *frame, int *tail);

void pack_engine      (can_frame_t *f, float rpm, float temp, float throttle);
void pack_abs         (can_frame_t *f, float s1, float s2, float s3, float s4, float slip);
void pack_transmission(can_frame_t *f, int gear, float torque);
void pack_battery     (can_frame_t *f, float voltage, float soc);

float get_engine_rpm     (const can_frame_t *f);
float get_engine_temp    (const can_frame_t *f);
float get_abs_slip       (const can_frame_t *f);
int   get_trans_gear     (const can_frame_t *f);
float get_battery_voltage(const can_frame_t *f);

#endif
