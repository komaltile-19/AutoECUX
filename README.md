# Automotive ECU Simulator using Virtual CAN Bus and IPC Mechanisms 

Problem Statement / Scenario

Modern vehicles use multiple Electronic Control Units (ECUs) to control engine, ABS, transmission, and battery systems. These ECUs continuously exchange sensor data using the CAN bus.

Testing real automotive systems requires expensive hardware and complex setups. This project solves that problem by creating a software-based ECU simulator using Linux system programming concepts.

The system simulates:

Real-time ECU communication
CAN bus data exchange
Fault detection
OBD-II diagnostics
Limp-home safety control

without requiring physical automotive hardware.
## Directory layout

```
automotive_ecu_simulator/
├── src/
│   ├── gateway.c          Supervisor: fork/execv/waitpid, signals, pipe
│   ├── ecu_engine.c       Engine ECU — rpm, temp, throttle (4 threads)
│   ├── ecu_abs.c          ABS ECU — wheel speed, slip ratio (4 threads)
│   ├── ecu_trans.c        Transmission ECU — gear, torque (4 threads)
│   ├── ecu_battery.c      Battery ECU — voltage, current, SoC (4 threads)
│   ├── fault_detector.c   CAN scanner + DTC classifier (2 threads)
│   ├── alert_manager.c    DTC escalation + limp-home dispatcher (2 threads)
│   ├── obd_logger.c       Pipe reader, file I/O: dtc.log + report.txt
│   └── dtc.c              DTC table and lookup functions
├── include/
│   ├── config.h           All constants: IPC names, paths, fault limits
│   ├── can_bus.h          CAN frame struct, ring buffer, pack/unpack helpers
│   ├── sensor.h           sensor_data_t, threshold_t, limp_cmd_t, ecu_sync_t
│   ├── dtc.h              dtc_msg_t, OBD-II severity levels
│   └── ecu_semaphore.h    Named POSIX semaphore API declarations
├── ipc/
│   ├── can_shared_memory.c  shm_open + mmap — virtual CAN bus ring buffer
│   ├── can_shared_memory.h  API declarations for shared memory module
│   ├── semaphore.c          sem_open — CAN bus write arbitration
│   ├── semaphore.h          Re-export of ecu_semaphore.h for ipc/ callers
│   ├── message_queue.c      mkfifo (DTC pipe) — fault alert channel
│   ├── message_queue.h      API declarations for DTC message queue
│   ├── fifo_comm.c          mkfifo (limp FIFO) — limp-home command stream
│   └── fifo_comm.h          API declarations for FIFO communication
├── config/
│   └── thresholds.conf    Runtime sensor limits (read by gateway at startup)
├── logs/                  Created at runtime
│   ├── dtc.log            Timestamped DTC events
│   └── report.txt         End-of-run fault count summary
├── build/                 Compiled binaries (created by make)
├── Makefile
└── README.md
```

---

Architecture Overview

The project follows a multi-process architecture where each ECU runs as an independent process.

Main Components
      Gateway Process
      Engine ECU
      ABS ECU
      Transmission ECU
      Battery ECU
      Fault Detector
      Alert Manager
      OBD Logger
Workflow
      ECUs generate sensor data.
      Data is written to shared memory CAN bus.
      Fault detector scans sensor values.
      DTC faults are generated.
      Alert manager sends limp-home commands.
      Logger stores system reports.


Process / Thread Design
Processes Used

    | Process        | Purpose                  |
| -------------- | ------------------------ |
| gateway        | System supervisor        |
| ecu_engine     | Engine sensor simulation |
| ecu_abs        | ABS sensor simulation    |
| ecu_trans      | Transmission simulation  |
| ecu_battery    | Battery monitoring       |
| fault_detector | Detects abnormal values  |
| alert_manager  | Sends safety commands    |
| obd_logger     | Stores logs and reports  |

Threads Used
ECU Threads

Each ECU contains:
      Sensor threads
      CAN writer thread
      Limp-home thread
Fault Detector Threads
      Scan thread
      Classifier thread
Alert Manager Threads
      Reader thread
      Process thread

IPC Mechanisms Used

| IPC Mechanism    | Purpose                         |
| ---------------- | ------------------------------- |
| Shared Memory    | Virtual CAN bus                 |
| POSIX Semaphores | Synchronization                 |
| Named FIFO Pipes | DTC and limp-home communication |
| Anonymous Pipe   | Logger communication            |
| Signals          | Fault injection and control     |


Shared Memory

/can_shm is used as a virtual CAN bus ring buffer between ECUs and fault detector.

FIFO Pipes
    /tmp/dtc_pipe → DTC messages
    /tmp/limp_pipe → Limp-home commands

Signal Handling Details

| Signal  | Purpose                  |
| ------- | ------------------------ |
| SIGUSR1 | Inject ECU faults        |
| SIGALRM | Send periodic snapshots  |
| SIGCHLD | Handle child termination |
| SIGINT  | Graceful shutdown        |
| SIGTERM | Stop all child processes |

Build Instructions

Compile Project
    make all

Clean Build Files
    make clean

Requirements:

    GCC
    Make
    Linux OS
    POSIX thread and semaphore support
  

Run Instructions
Start Simulator
    make run
Stop Simulator
    make stop
Inject Faults
    make test
  

Conclusion

The Automotive ECU Simulator demonstrates how multiple ECUs communicate through a virtual CAN bus using Linux IPC mechanisms. The project successfully implements fault detection, OBD-II diagnostics, and limp-home safety control using shared memory, semaphores, threads, FIFOs, and signals.

It provides practical knowledge of:

    Embedded systems
    Linux system programming
    Inter-process communication
    Automotive diagnostics
    Real-time fault handling