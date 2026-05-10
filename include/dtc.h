#ifndef DTC_H
#define DTC_H

#include <stdint.h>
#include "config.h"

/* Severity levels */
#define CRITICAL   3
#define WARNING    2
#define INFO       1

/* -----------------------------------------------------------------------
   DTC message passed between fault_detector → alert_manager
   ----------------------------------------------------------------------- */
typedef struct {
    char     code[10];
    uint32_t ecu_id;
    uint8_t  severity;
    char     description[50];
    uint64_t timestamp_us;
} dtc_msg_t;

/* -----------------------------------------------------------------------
   Static DTC table entry
   ----------------------------------------------------------------------- */
typedef struct {
    uint32_t ecu_id;
    char     code[10];
    uint8_t  severity;
    char     description[50];
} dtc_entry_t;

/* -----------------------------------------------------------------------
   DTC lookup — returns pointer into dtc_table or NULL
   ----------------------------------------------------------------------- */
dtc_entry_t *find_dtc(uint32_t ecu_id);

/* Returns printable severity string */
const char  *get_severity(uint8_t sev);

#endif
