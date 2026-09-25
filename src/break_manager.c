#include "common.h"

int check_break_quota(int *items_since_break, int *current_threshold, char *reason_buf, size_t buf_size) {
    (*items_since_break)++;
    if (*items_since_break >= *current_threshold) {
        snprintf(reason_buf, buf_size, "Batch quota reached: %d items", *items_since_break);
        *items_since_break = 0;
        *current_threshold = BREAK_BATCH_MIN + (rand() % (BREAK_BATCH_MAX - BREAK_BATCH_MIN + 1));
        return 1;
    }
    return 0;
}

void take_break(const char *worker_name, const char *reason) {
    sem_t *sem = sem_open(SEM_NAME, 0);
    if (sem == SEM_FAILED) {
        perror("Failed to attach to break semaphore");
        return;
    }

    printf("  %s requesting break [%s]... (waiting for semaphore)\n", worker_name, reason);

    if (sem_wait(sem) == -1) {
        perror("sem_wait failed");
        sem_close(sem);
        return;
    }

    // Critical section begins: exactly one worker in the break zone
    FILE *log_fp = fopen(LOG_FILE, "a");
    time_t rawtime;
    struct tm *timeinfo;
    char time_str[32];

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", timeinfo);

    printf("  %s entered break room at %s (reason: %s, semaphore locked)\n",
           worker_name, time_str, reason);

    if (log_fp) {
        fprintf(log_fp, "[%s | PID %d] START break at %s | Reason: %s\n",
                worker_name, getpid(), time_str, reason);
        fflush(log_fp);
    }

    // Simulate break duration (1 second)
    sleep(BREAK_DURATION_SEC);

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", timeinfo);

    printf("  %s ended break at %s (semaphore released)\n", worker_name, time_str);
    if (log_fp) {
        fprintf(log_fp, "[%s | PID %d] END   break at %s\n", 
            worker_name, getpid(), time_str);
        fclose(log_fp);
    }

    // Critical section ends: release semaphore
    if (sem_post(sem) == -1) {
        perror("sem_post failed");
    }

    sem_close(sem);
}