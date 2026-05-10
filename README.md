# Automotive ECU Simulator using Virtual CAN Bus and IPC Mechanisms

---

# Problem Statement / Scenario

Modern vehicles use multiple Electronic Control Units (ECUs) to control engine, ABS, transmission, and battery systems. These ECUs continuously exchange sensor data using the CAN bus.

Testing real automotive systems requires expensive hardware and complex setups. This project solves that problem by creating a software-based ECU simulator using Linux system programming concepts.

The system simulates:

- Real-time ECU communication
- CAN bus data exchange
- Fault detection
- OBD-II diagnostics
- Limp-home safety control

without requiring physical automotive hardware.

---

# Directory Layout

```bash
automotive_ecu_simulator/
├── src/
│   ├── gateway.c
│   ├── ecu_engine.c
│   ├── ecu_abs.c
│   ├── ecu_trans.c
│   ├── ecu_battery.c
│   ├── fault_detector.c
│   ├── alert_manager.c
│   ├── obd_logger.c
│   └── dtc.c
│
├── include/
│   ├── config.h
│   ├── can_bus.h
│   ├── sensor.h
│   ├── dtc.h
│   └── ecu_semaphore.h
│
├── ipc/
│   ├── can_shared_memory.c
│   ├── can_shared_memory.h
│   ├── semaphore.c
│   ├── semaphore.h
│   ├── message_queue.c
│   ├── message_queue.h
│   ├── fifo_comm.c
│   └── fifo_comm.h
│
├── config/
│   └── thresholds.conf
│
├── logs/
│   ├── dtc.log
│   └── report.txt
│
├── build/
├── Makefile
└── README.md
```

---

# Architecture Overview

The project follows a multi-process architecture where each ECU runs as an independent process.

## Main Components

- Gateway Process
- Engine ECU
- ABS ECU
- Transmission ECU
- Battery ECU
- Fault Detector
- Alert Manager
- OBD Logger

## Workflow

1. ECUs generate sensor data.
2. Data is written to shared memory CAN bus.
3. Fault detector scans sensor values.
4. DTC faults are generated.
5. Alert manager sends limp-home commands.
6. Logger stores system reports.

---

# Process / Thread Design

## Processes Used

| Process | Purpose |
|---|---|
| gateway | System supervisor |
| ecu_engine | Engine sensor simulation |
| ecu_abs | ABS sensor simulation |
| ecu_trans | Transmission simulation |
| ecu_battery | Battery monitoring |
| fault_detector | Detects abnormal values |
| alert_manager | Sends safety commands |
| obd_logger | Stores logs and reports |

---

## Threads Used

### ECU Threads

Each ECU contains:
- Sensor threads
- CAN writer thread
- Limp-home thread

### Fault Detector Threads

- Scan thread
- Classifier thread

### Alert Manager Threads

- Reader thread
- Process thread

---

# IPC Mechanisms Used

| IPC Mechanism | Purpose |
|---|---|
| Shared Memory | Virtual CAN bus |
| POSIX Semaphores | Synchronization |
| Named FIFO Pipes | DTC and limp-home communication |
| Anonymous Pipe | Logger communication |
| Signals | Fault injection and control |

---

## Shared Memory

`/can_shm` is used as a virtual CAN bus ring buffer between ECUs and the fault detector.

---

## FIFO Pipes

- `/tmp/dtc_pipe` → DTC messages
- `/tmp/limp_pipe` → Limp-home commands

---

# Signal Handling Details

| Signal | Purpose |
|---|---|
| SIGUSR1 | Inject ECU faults |
| SIGALRM | Send periodic snapshots |
| SIGCHLD | Handle child termination |
| SIGINT | Graceful shutdown |
| SIGTERM | Stop all child processes |

---

# Build Instructions

## Compile Project

```bash
make all
```

## Clean Build Files

```bash
make clean
```

## Requirements

- GCC
- Make
- Linux OS
- POSIX thread and semaphore support

---

# Run Instructions

## Start Simulator

```bash
make run
```

## Stop Simulator

```bash
make stop
```

## Inject Faults

```bash
make test
```

---

# Conclusion

The Automotive ECU Simulator demonstrates how multiple ECUs communicate through a virtual CAN bus using Linux IPC mechanisms.

The project successfully implements:
- Fault detection
- OBD-II diagnostics
- Limp-home safety control
- Shared memory communication
- FIFO-based IPC
- Multi-threading and synchronization

It provides practical knowledge of:
- Embedded systems
- Linux system programming
- Inter-process communication
- Automotive diagnostics
- Real-time fault handling
