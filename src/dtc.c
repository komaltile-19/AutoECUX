#include <stddef.h>
#include "../include/dtc.h"

/* -----------------------------------------------------------------------
   Static DTC table
   ----------------------------------------------------------------------- */
static dtc_entry_t dtc_table[] = {
    { CAN_ID_ENGINE,  "P0217", CRITICAL, "Engine Over Temperature" },
    { CAN_ID_ENGINE,  "P0300", WARNING,  "Engine Misfire"         },
    { CAN_ID_ABS,     "C0040", CRITICAL, "ABS Sensor Fault"       },
    { CAN_ID_TRANS,   "P0700", WARNING,  "Transmission Fault"     },
    { CAN_ID_BATTERY, "P0562", CRITICAL, "Low Battery Voltage"    },
};

#define DTC_TABLE_SIZE ((int)(sizeof(dtc_table) / sizeof(dtc_table[0])))

dtc_entry_t *find_dtc(uint32_t ecu_id)
{
    for (int i = 0; i < DTC_TABLE_SIZE; i++) {
        if (dtc_table[i].ecu_id == ecu_id)
            return &dtc_table[i];
    }
    return NULL;
}

const char *get_severity(uint8_t sev)
{
    switch (sev) {
        case CRITICAL: return "CRITICAL";
        case WARNING:  return "WARNING";
        default:       return "INFO";
    }
}
