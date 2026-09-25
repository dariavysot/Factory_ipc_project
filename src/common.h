/**
 * @file common.h
 * @brief Common definitions, packet structures, and domain constants for the factory pipeline.
 */

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

/* IPC Configuration Constants */
#define FIFO_PATH           "/tmp/factory_fifo"
#define SEM_NAME            "/factory_break_sem"
#define BREAK_LOG_FILE      "break_log.txt"
#define PROJECT_ID          'F' // Project identifier for ftok() message queue key

/* Production Item Constants */
#define SERIAL_MIN                  10000
#define SERIAL_RANGE                90000       /* Yields 10000..99999 */
#define QUALITY_MIN                 1
#define QUALITY_MAX                 10
#define DEFECT_PROBABILITY_PERCENT  15          /* 15% defect chance during primary inspection */

/* Item Status Strings */
#define STATUS_STANDARD     "STANDARD"
#define STATUS_DEFECT       "DEFECT"

/* Break Management Limits */
#define BREAK_DURATION_SEC  4           /* Duration of worker rest in seconds */
#define BREAK_BATCH_MIN     5           /* Minimum items between breaks */
#define BREAK_BATCH_MAX     7           /* Maximum items between breaks */

/* IPC Packet Structures */

/** Unnamed pipe packet sent from Supervisor to Worker 1 */
typedef struct {
    int serial_number; // Randomly generated item serial number
} pipe_packet_t;

/** FIFO streaming packet passed between Worker 1 and Worker 2 */
typedef struct {
    int serial_number; // Serial number
    char status[16];   // "STANDARD" or "DEFECT"
} fifo_packet_t;

/** System V Message Queue packet for quality reporting back to Supervisor */
typedef struct {
    long mtype;        // Mandatory message type for msgsnd/msgrcv (must be > 0)
    int serial_number; // Serial number of the approved item
    int quality_score; // Final quality testing score (range: 1 - 10)
} mq_packet_t;

/** Bundle of supervisor-managed IPC channel descriptors */
typedef struct {
    int msqid;
    sem_t *break_sem;
    int pipe_fd[2];
} supervisor_ipc_t;

/* Worker Station Entry Points */
void run_worker1(int read_fd);
void run_worker2(int msqid);

/* Break management functions */
void take_break(const char *worker_name, const char *reason);
int check_break_quota(int *items_since_break, int *current_threshold, char *reason_buf, size_t buf_size);
void log_break_event(const char *worker_name, pid_t pid, const char *event_type, const char *reason);

#endif // COMMON_H