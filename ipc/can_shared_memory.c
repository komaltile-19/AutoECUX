#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../include/can_bus.h"
#include "../include/config.h"

/* SHM_NAME comes from config.h ("/can_shm") */

static can_bus_t *shared_bus = NULL;

/* -----------------------------------------------------------------------
   Create shared memory segment (called by gateway)
   ----------------------------------------------------------------------- */
can_bus_t *can_shm_create(void)
{
    shm_unlink(SHM_NAME);

    int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (fd < 0) {
        perror("shm_open create");
        return NULL;
    }

    if (ftruncate(fd, sizeof(can_bus_t)) < 0) {
        perror("ftruncate");
        close(fd);
        return NULL;
    }

    shared_bus = mmap(NULL, sizeof(can_bus_t),
                      PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);

    if (shared_bus == MAP_FAILED) {
        perror("mmap create");
        return NULL;
    }

    memset(shared_bus, 0, sizeof(can_bus_t));
    printf("[can_shm] Created %s size=%zu bytes slots=%d\n",
           SHM_NAME, sizeof(can_bus_t), CAN_BUS_SLOTS);
    return shared_bus;
}

/* -----------------------------------------------------------------------
   Attach to existing shared memory (called by ECU processes)
   ----------------------------------------------------------------------- */
can_bus_t *can_shm_attach(void)
{
    int fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (fd < 0) {
        perror("shm_open attach");
        return NULL;
    }

    shared_bus = mmap(NULL, sizeof(can_bus_t),
                      PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);

    if (shared_bus == MAP_FAILED) {
        perror("mmap attach");
        return NULL;
    }

    return shared_bus;
}

/* -----------------------------------------------------------------------
   Detach (unmap) without removing the segment
   ----------------------------------------------------------------------- */
void can_shm_detach(can_bus_t *bus)
{
    if (bus && bus != MAP_FAILED)
        munmap(bus, sizeof(can_bus_t));
}

/* -----------------------------------------------------------------------
   Unmap and unlink (called by gateway on shutdown)
   ----------------------------------------------------------------------- */
void can_shm_destroy(void)
{
    if (shared_bus && shared_bus != MAP_FAILED) {
        munmap(shared_bus, sizeof(can_bus_t));
        shared_bus = NULL;
    }
    shm_unlink(SHM_NAME);
    printf("[can_shm] Removed %s\n", SHM_NAME);
}

/* -----------------------------------------------------------------------
   Write one CAN frame into the ring buffer (head advances)
   ----------------------------------------------------------------------- */
int can_write(can_bus_t *bus, can_frame_t *frame)
{
    if (!bus || !frame)
        return -1;

    int idx = bus->head % CAN_BUS_SLOTS;
    bus->frames[idx] = *frame;
    bus->head = (bus->head + 1) % CAN_BUS_SLOTS;
    return 0;
}

/* -----------------------------------------------------------------------
   Read one CAN frame (caller owns the tail index)
   Returns 1 if a frame was read, 0 if the buffer is empty.
   ----------------------------------------------------------------------- */
int can_read(can_bus_t *bus, can_frame_t *frame, int *tail)
{
    if (!bus || !frame || !tail)
        return 0;

    if (*tail == bus->head)
        return 0;

    int idx = (*tail) % CAN_BUS_SLOTS;
    *frame = bus->frames[idx];
    *tail  = (*tail + 1) % CAN_BUS_SLOTS;
    return 1;
}

/* -----------------------------------------------------------------------
   Pack helpers
   ----------------------------------------------------------------------- */
void pack_engine(can_frame_t *f, float rpm, float temp, float throttle)
{
    if (!f) return;
    uint16_t irpm = (uint16_t)(rpm);
    f->data[0] = (uint8_t)(irpm >> 8);
    f->data[1] = (uint8_t)(irpm & 0xFF);
    f->data[2] = (uint8_t)(temp);
    f->data[3] = (uint8_t)(throttle);
    f->dlc       = 4;
    f->can_id    = CAN_ID_ENGINE;
    f->timestamp = get_time_us();
    f->valid     = 1;
}

void pack_abs(can_frame_t *f,
              float s1, float s2, float s3, float s4, float slip)
{
    if (!f) return;
    f->data[0] = (uint8_t)(s1 * 2.0f);
    f->data[1] = (uint8_t)(s2 * 2.0f);
    f->data[2] = (uint8_t)(s3 * 2.0f);
    f->data[3] = (uint8_t)(s4 * 2.0f);
    f->data[4] = (uint8_t)(slip * 255.0f);
    f->dlc       = 5;
    f->can_id    = CAN_ID_ABS;
    f->timestamp = get_time_us();
    f->valid     = 1;
}

void pack_transmission(can_frame_t *f, int gear, float torque)
{
    if (!f) return;
    uint16_t itorque = (uint16_t)(torque);
    f->data[0] = (uint8_t)(gear);
    f->data[1] = (uint8_t)(itorque >> 8);
    f->data[2] = (uint8_t)(itorque & 0xFF);
    f->dlc       = 3;
    f->can_id    = CAN_ID_TRANS;
    f->timestamp = get_time_us();
    f->valid     = 1;
}

void pack_battery(can_frame_t *f, float voltage, float soc)
{
    if (!f) return;
    /* voltage encoded as voltage * 10 → fits in uint8 for 0–25.5 V */
    f->data[0] = (uint8_t)(voltage * 10.0f);
    f->data[1] = (uint8_t)(soc);
    f->dlc       = 2;
    f->can_id    = CAN_ID_BATTERY;
    f->timestamp = get_time_us();
    f->valid     = 1;
}

/* -----------------------------------------------------------------------
   Unpack helpers
   ----------------------------------------------------------------------- */
float get_engine_rpm(const can_frame_t *f)
{
    if (!f) return 0.0f;
    return (float)((((uint16_t)f->data[0]) << 8) | f->data[1]);
}

float get_engine_temp(const can_frame_t *f)
{
    if (!f) return 0.0f;
    return (float)f->data[2];
}

float get_abs_slip(const can_frame_t *f)
{
    if (!f) return 0.0f;
    return (float)f->data[4] / 255.0f;
}

int get_trans_gear(const can_frame_t *f)
{
    if (!f) return 0;
    return (int)f->data[0];
}

float get_battery_voltage(const can_frame_t *f)
{
    if (!f) return 0.0f;
    return (float)f->data[0] / 10.0f;
}
