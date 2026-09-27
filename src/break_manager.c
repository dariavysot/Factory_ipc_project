/**
 * @file break_manager.c
 * @brief Thread/process break synchronization and logging via POSIX Named Semaphore.
 */

#include "common.h"

/**
 * Appends a formatted timestamped break entry to the persistent break log.
 */
void log_break_event(const char *worker_name, pid_t pid, const char *event_type, const char *reason) {
    FILE *log_file = fopen(BREAK_LOG_FILE, "a");
    if (!log_file) {
        perror("[ERROR] Unable to open break log file");
        return;
    }

    time_t raw_time = time(NULL);
    struct tm *time_info = localtime(&raw_time);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", time_info);

    if (reason && strlen(reason) > 0) {
        fprintf(log_file, "[%s | PID %d] %-5s break at %s | Reason: %s\n",
                worker_name, pid, event_type, time_str, reason);
    } else {
        fprintf(log_file, "[%s | PID %d] %-5s break at %s\n",
                worker_name, pid, event_type, time_str);
    }

    fclose(log_file);
}

/**
 * Evaluates whether an item quota threshold has been reached.
 * Automatically increments the progress counter and resets it upon triggering.
 */
int check_break_quota(int *items_since_break, int *current_threshold, char *reason_buf, size_t buf_size) {
    (*items_since_break)++;
    if (*items_since_break >= *current_threshold) {
        snprintf(reason_buf, buf_size, "Batch quota reached: %d items", *current_threshold);
        *items_since_break = 0;
        *current_threshold = BREAK_BATCH_MIN + (rand() % (BREAK_BATCH_MAX - BREAK_BATCH_MIN + 1));
        return 1;
    }
    return 0;
}

/**
 * Enters the mutual exclusion break room protected by POSIX Semaphore.
 */
void take_break(const char *worker_name, const char *reason) {
    pid_t pid = getpid();
    printf("  %s requesting break [%s]... (waiting for semaphore)\n", worker_name, reason);

    // Open existing named semaphore
    sem_t *sem = sem_open(SEM_NAME, 0);
    if (sem == SEM_FAILED) {
        perror("[ERROR] Failed to open semaphore in take_break");
        return;
    }

    // Enter critical section (mutual exclusion)
    if (sem_wait(sem) == -1) {
        perror("[ERROR] sem_wait failed");
        sem_close(sem);
        return;
    }

    time_t start_time = time(NULL);
    struct tm *st_info = localtime(&start_time);
    char start_str[32];
    strftime(start_str, sizeof(start_str), "%Y-%m-%d %H:%M:%S", st_info);

    printf("  %s entered break room at %s (reason: %s, semaphore locked)\n",
           worker_name, start_str, reason);
    log_break_event(worker_name, pid, "START", reason);

    // Simulate break duration
    sleep(BREAK_DURATION_SEC);

    time_t end_time = time(NULL);
    struct tm *et_info = localtime(&end_time);
    char end_str[32];
    strftime(end_str, sizeof(end_str), "%Y-%m-%d %H:%M:%S", et_info);

    printf("  %s ended break at %s (semaphore released)\n", worker_name, end_str);
    log_break_event(worker_name, pid, "END", NULL);

    // Leave critical section
    if (sem_post(sem) == -1) {
        perror("[ERROR] sem_post failed");
    }

    sem_close(sem);
}