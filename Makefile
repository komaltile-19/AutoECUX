CC      = gcc
CFLAGS  = -Wall -Wextra -g -I include -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
LIBS    = -pthread -lrt

BUILD   = build
IPC_SRC = ipc/can_shared_memory.c ipc/semaphore.c \
          ipc/message_queue.c     ipc/fifo_comm.c
DTC_SRC = src/dtc.c

.PHONY: all run test stop clean

all: $(BUILD) \
     $(BUILD)/gateway       \
     $(BUILD)/ecu_engine    \
     $(BUILD)/ecu_abs       \
     $(BUILD)/ecu_trans     \
     $(BUILD)/ecu_battery   \
     $(BUILD)/fault_detector\
     $(BUILD)/alert_manager \
     $(BUILD)/obd_logger

$(BUILD):
	mkdir -p $(BUILD) logs

# --- gateway (no IPC object files — it only creates resources) ---
$(BUILD)/gateway: src/gateway.c $(IPC_SRC) $(DTC_SRC)
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

# --- ECU processes ---
$(BUILD)/ecu_engine: src/ecu_engine.c $(IPC_SRC) $(DTC_SRC)
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

$(BUILD)/ecu_abs: src/ecu_abs.c $(IPC_SRC) $(DTC_SRC)
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

$(BUILD)/ecu_trans: src/ecu_trans.c $(IPC_SRC) $(DTC_SRC)
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

$(BUILD)/ecu_battery: src/ecu_battery.c $(IPC_SRC) $(DTC_SRC)
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

# --- Fault detector and alert manager ---
$(BUILD)/fault_detector: src/fault_detector.c $(IPC_SRC) $(DTC_SRC)
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

$(BUILD)/alert_manager: src/alert_manager.c $(IPC_SRC) $(DTC_SRC)
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

# --- OBD logger (no IPC, no DTC lib) ---
$(BUILD)/obd_logger: src/obd_logger.c
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

# --- Run the full simulation (gateway manages child processes) ---
run: all
	@echo "Starting ECU Simulator (Ctrl-C to stop)..."
	./$(BUILD)/gateway

# --- Inject a fault into every ECU (run while simulation is active) ---
test:
	@echo "Injecting faults..."
	-kill -USR1 $$(pgrep -x ecu_engine)  2>/dev/null || true
	-kill -USR1 $$(pgrep -x ecu_abs)     2>/dev/null || true
	-kill -USR1 $$(pgrep -x ecu_battery) 2>/dev/null || true
	-kill -USR1 $$(pgrep -x ecu_trans)   2>/dev/null || true

# --- Kill all simulator processes and clean up IPC ---
stop:
	-pkill -x gateway       2>/dev/null || true
	-pkill -x ecu_engine    2>/dev/null || true
	-pkill -x ecu_abs       2>/dev/null || true
	-pkill -x ecu_trans     2>/dev/null || true
	-pkill -x ecu_battery   2>/dev/null || true
	-pkill -x fault_detector 2>/dev/null || true
	-pkill -x alert_manager  2>/dev/null || true
	-pkill -x obd_logger     2>/dev/null || true
	-rm -f /tmp/dtc_pipe /tmp/limp_pipe 2>/dev/null || true
	-rm -f /dev/shm/can_shm             2>/dev/null || true

# --- Remove compiled binaries and log files ---
clean:
	rm -rf $(BUILD) logs
