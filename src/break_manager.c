#include "common.h"

void take_break(const char *worker_name) {
    sem_t *sem = sem_open(SEM_NAME, 0);
    if (sem == SEM_FAILED) {
        perror("Failed to attach to break semaphore");
        return;
    }

    printf("  %s requesting break... (waiting for semaphore)\n", worker_name);

    // Wait until break room is free (semaphore value becomes 1, then decrement to 0)
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

    printf("  %s entered break room at %s (semaphore locked)\n", worker_name, time_str);
    if (log_fp) {
        fprintf(log_fp, "[%s | PID %d] START break at %s\n", worker_name, getpid(), time_str);
        fflush(log_fp);
    }

    // Simulate break duration (1 second)
    sleep(1);

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", timeinfo);

    printf("  %s ended break at %s (semaphore released)\n", worker_name, time_str);
    if (log_fp) {
        fprintf(log_fp, "[%s | PID %d] END   break at %s\n", worker_name, getpid(), time_str);
        fclose(log_fp);
    }

    // Critical section ends: release semaphore
    if (sem_post(sem) == -1) {
        perror("sem_post failed");
    }

    sem_close(sem);
}