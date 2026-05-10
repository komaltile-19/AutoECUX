#ifndef CONFIG_H
#define CONFIG_H

/* System */
#define SYSTEM_NAME    "ECU Fault Detector"

/* Shared memory and semaphore names */
#define SHM_NAME       "/can_shm"
#define SEM_NAME       "/can_sem"

/* Named FIFO paths */
#define DTC_PIPE       "/tmp/dtc_pipe"
#define LIMP_PIPE      "/tmp/limp_pipe"

/* CAN ring buffer size */
#define CAN_BUS_SLOTS  64

/* CAN IDs — match can_bus.h */
#define CAN_ID_ENGINE   100
#define CAN_ID_ABS      101
#define CAN_ID_TRANS    102
#define CAN_ID_BATTERY  103

/* Aliases used by ECU source files */
#define ENGINE_ID      CAN_ID_ENGINE
#define ABS_ID         CAN_ID_ABS
#define TRANS_ID       CAN_ID_TRANS
#define BATTERY_ID     CAN_ID_BATTERY

/* Pipe ends */
#define READ_END       0
#define WRITE_END      1

/* Log files */
#define DTC_LOG_FILE   "logs/dtc.log"
#define TRACE_FILE     "logs/can_trace.csv"

/* Sensor timing (nanoseconds for nanosleep) */
#define SENSOR_DELAY   50000000L

/* Fault thresholds (defaults; overridden by thresholds.conf) */
#define HIGH_RPM       6500.0f
#define HIGH_TEMP      105.0f
#define LOW_VOLTAGE    11.0f

/* Limp mode RPM cap */
#define RPM_LIMIT      3000.0f

/* Config file path */
#define CONFIG_THRESHOLD_FILE "config/thresholds.conf"

#endif
