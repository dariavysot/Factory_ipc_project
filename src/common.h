#ifndef COMMON_H
#define COMMON_H

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <semaphore.h>
#include <time.h>

/* --- System paths and IPC constants --- */
#define FIFO_PATH "/tmp/factory_fifo"
#define SEM_NAME  "/factory_break_sem"
#define LOG_FILE  "break_log.txt"
#define PROJECT_ID 'F' // Project identifier for ftok() message queue key

/* Defect probability rate for Worker 1 (15%) */
#define DEFECT_PROBABILITY_PERCENT 15

/* Product status markers for named pipe (FIFO) communication */
#define STATUS_STANDARD "STANDARD"
#define STATUS_DEFECT   "DEFECT"

/* 
 * 1. Data structure for the unnamed pipe
 * Direction: Supervisor (Parent) -> Worker 1 (Child 1)
 */
typedef struct {
    int serial_number; // Randomly generated item serial number
} pipe_packet_t;

/* 
 * 2. Data structure for the named pipe (FIFO)
 * Direction: Worker 1 (Child 1) -> Worker 2 (Child 2)
 */
typedef struct {
    int serial_number; // Serial number
    char status[16];   // "STANDARD" or "DEFECT"
} fifo_packet_t;

/* 
 * 3. Data structure for the System V Message Queue
 * Direction: Worker 2 (Child 2) -> Supervisor (Parent)
 */
typedef struct {
    long mtype;        // Mandatory message type for msgsnd/msgrcv (must be > 0)
    int serial_number; // Serial number of the approved item
    int quality_score; // Final quality testing score (range: 1 - 10)
} mq_packet_t;

/* Worker process entry points */
void run_worker1(int read_fd);
void run_worker2(int mtype);

#endif // COMMON_H