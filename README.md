# Навчальний проєкт № 1: "Завод"

Курс: **Сучасні технології системного програмування** (ФІТ, КНУ, 2026)  
Виконала: студентка групи ІПЗ-32 Висоцька Д. І.  
Викладач: Лукашов О. В.

A multi-process factory simulation pipeline written in C for POSIX/UNIX environments. The system models a production assembly line using a **Supervisor-Worker** architecture with diverse Inter-Process Communication (IPC) primitives for task coordination, streaming, mutual exclusion, and reporting.

## Environment & Requirements

- **OS:** Linux (Ubuntu 20.04+ / Debian / WSL2 Ubuntu under Windows)
- **Compiler:** GCC (supports C99/C11, `_POSIX_C_SOURCE 200809L`)
- **Libraries:** Standard C Library (`glibc`), POSIX Threads (`-lpthread` / `librt` for semaphore handling)
- **Build System:** GNU Make

## Architectural Model

```text
                  +-------------------+
                  |    Supervisor     |
                  +-------------------+
                     |             ^
       Unnamed Pipe  |             |  System V Message Queue
                     |             |  (tested quality results)
                     v             |
                +----------+  FIFO +----------+
                | Worker 1 | ===== | Worker 2 |
                +----------+ stream +----------+
                     \             /
                      \           /
               +-------------------------+
               | POSIX Named Semaphore   |
               |   (/factory_break_sem)  |
               |   Mutual Exclusion Room |
               +-------------------------+
```
### Components

1. **Supervisor (`src/main.c`)**:
   - Creates and initializes all IPC facilities.
   - Blocks signals until workers are spawned, then suspends execution (`sigsuspend`) awaiting `SIGUSR1` and `SIGUSR2` handshake notifications.
   - Dispatches items by writing `pipe_packet_t` into an **Unnamed Pipe** and closes the write descriptor to trigger `EOF`.
   - Reclaims child processes with `waitpid()`.
   - Drains final quality scores from the **System V Message Queue**, presents a consolidated statistical report, and cleans up all IPC resources.

2. **Worker 1 (`src/worker1.c`) — Primary Inspection**:
   - Opens the **Named Pipe (FIFO)** for writing and signals readiness via `SIGUSR1`.
   - Reads serialized items from the unnamed pipe.
   - Simulates visual inspection: flags defects (`DEFECT`) or standards (`STANDARD`) based on configured probability thresholds.
   - Writes inspected packets (`fifo_packet_t`) into the FIFO.
   - Tracks fatigue workload and triggers synchronized rest periods.

3. **Worker 2 (`src/worker2.c`) — Diagnostic Testing**:
   - Opens the FIFO for reading and signals readiness via `SIGUSR2`.
   - Consumes items from the FIFO until `EOF` (Worker 1 closes station).
   - Filters out defect items; conducts diagnostics on standard items, assigning a quality rating (1–10).
   - Forwards test reports (`mq_packet_t`) to the Supervisor via a **System V Message Queue**.
   - Competes with Worker 1 for the break room under identical fatigue constraints.

4. **Break Manager (`src/break_manager.c`)**:
   - Provides thread-safe / process-safe break logic using a POSIX Named Semaphore (`/factory_break_sem`).
   - Limits break room occupancy to 1 worker at a time (Mutual Exclusion / Mutex).
   - Logs all entry (`START`) and exit (`END`) events with timestamps into `break_log.txt`.


## IPC Mechanisms Summary

| Primitive | Path / Key | Role |
| :--- | :--- | :--- |
| **UNIX Signals** | `SIGUSR1`, `SIGUSR2` | Process synchronization & readiness handshake |
| **Unnamed Pipe** | Descriptor pair `pipe_fd` | One-way stream for item dispatch (Supervisor $\to$ Worker 1) |
| **Named Pipe (FIFO)** | `/tmp/factory_fifo` | Intermediate item pipeline (Worker 1 $\to$ Worker 2) |
| **System V Message Queue** | Key: `ftok(".", 'F')` | Quality verification channel (Worker 2 $\to$ Supervisor) |
| **POSIX Named Semaphore** | `/factory_break_sem` | Mutual exclusion lock for the worker break room |


## Compilation and Execution

### Build
```bash
make clean && make
```

## Run Simulation
Pass the number of items to produce as a command-line argument:


### Run with 10 items
```bash
./factory 10
```

### Display usage
```bash
./factory --help
```
## Integration Testing

The project includes an automated suite of 4 integration tests covering each IPC mechanism and synchronization constraint:

| Test Script | Target Subsystem | Verification Criteria |
| :--- | :--- | :--- |
| `tests/test_pipe.sh` | Unnamed Pipe | Confirms zero data loss for all dispatched packets between Supervisor and Worker 1. |
| `tests/test_fifo.sh` | Named Pipe (FIFO) | Validates inter-worker streaming, packet integrity, and correct routing of `STANDARD` vs. `DEFECT` items. |
| `tests/test_mq.sh` | System V Message Queue | Ensures 1:1 parity between quality reports dispatched by Worker 2 and collected by Supervisor, plus clean queue deallocation. |
| `tests/test_semaphore.sh` | POSIX Named Semaphore | Proves mutual exclusion (strict `START`/`END` parity, non-overlapping critical sections) and sanity of quota-driven break intervals. |

### Running the Tests

Grant execution permissions and run individual test scripts:

```bash
chmod +x tests/*.sh

# Run all tests sequentially
./tests/test_pipe.sh
./tests/test_fifo.sh
./tests/test_mq.sh
./tests/test_semaphore.sh
```
