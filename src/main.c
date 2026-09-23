#include "common.h"
#include <signal.h>
#include <sys/wait.h>

/* Atomic readiness flag for Worker 1 */
static volatile sig_atomic_t worker1_ready = 0;

static void print_usage(FILE *stream, const char *prog_name) {
    fprintf(stream, "Usage: %s <total_items>\n", prog_name);
    fprintf(stream, "       %s -h | --help\n\n", prog_name);
    fprintf(stream, "Arguments:\n");
    fprintf(stream, "  <total_items>  Positive integer specifying total items to process\n\n");
    fprintf(stream, "Options:\n");
    fprintf(stream, "  -h, --help     Display this help message and exit\n");
}


/* Handler for SIGUSR1 (readiness signal from Worker 1) */
static void handle_worker1_ready(int sig) {
    (void)sig;
    worker1_ready = 1;
}

/* Configure signal handling for supervisor */
static void setup_supervisor_signals(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_worker1_ready;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("[ERROR] Failed to configure SIGUSR1 handler");
        exit(EXIT_FAILURE);
    }

}
 
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "[ERROR] Missing required argument.\n\n");
        print_usage(stderr, argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        print_usage(stdout, argv[0]);
        return EXIT_SUCCESS;
    }

    if (argc > 2) {
        fprintf(stderr, "[ERROR] Too many arguments provided.\n\n");
        print_usage(stderr, argv[0]);
        return EXIT_FAILURE;
    }

    char *endptr = NULL;
    errno = 0;
    long total_items = strtol(argv[1], &endptr, 10);

    if (errno != 0 || *endptr != '\0' || total_items <= 0) {
        fprintf(stderr, "[ERROR] Invalid number of items: '%s'. Must be a positive integer.\n\n", argv[1]);
        print_usage(stderr, argv[0]);
        return EXIT_FAILURE;
    }

    setup_supervisor_signals();

   printf("[Supervisor] Process started. PID: %d\n", getpid());
    printf("[Supervisor] Target items count to produce: %ld\n\n", total_items);

    // Fork Worker 1
    pid_t pid1 = fork();
    if (pid1 == -1) {
        perror("[ERROR] Failed to fork Worker 1");
        return EXIT_FAILURE;
    }

    if (pid1 == 0) {
        // Child 1 logic
        printf("  [Worker 1] Started. PID: %d, PPID: %d\n", getpid(), getppid());
        printf("  [Worker 1] Initializing station...\n");
        sleep(2); // 2s initialization simulation

        printf("  [Worker 1] Sending SIGUSR1 readiness signal to Supervisor...\n");
        if (kill(getppid(), SIGUSR1) == -1) {
            perror("  [Worker 1] Failed to send SIGUSR1");
            _exit(EXIT_FAILURE);
        }

        _exit(EXIT_SUCCESS);
    }

    // Supervisor waiting for Worker 1 readiness signal
    printf("[Supervisor] Awaiting readiness signal from Worker 1...\n");
    while (!worker1_ready) {
        pause();
    }
    printf("[Supervisor] Received SIGUSR1: Worker 1 is ready for production!\n\n");

    // Reap Worker 1 process
    int status;
    waitpid(pid1, &status, 0);

    printf("[Supervisor] All tasks completed. Exiting.\n");
    return EXIT_SUCCESS;
}