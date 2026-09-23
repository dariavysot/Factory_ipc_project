#include "common.h"
#include <signal.h>
#include <sys/wait.h>

/* Atomic readiness flag for Worker 1 */
static volatile sig_atomic_t worker1_ready = 0;
static volatile sig_atomic_t worker2_ready = 0;

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

/* Handler for SIGUSR2 (Worker 2 readiness) */
static void handle_worker2_ready(int sig) {
    (void)sig;
    worker2_ready = 1;
}

/* Configure signal handling for supervisor */
static void setup_supervisor_signals(void) {
    struct sigaction sa1;
    memset(&sa1, 0, sizeof(sa1));
    sa1.sa_handler = handle_worker1_ready;
    sigemptyset(&sa1.sa_mask);
    sa1.sa_flags = 0;

    if (sigaction(SIGUSR1, &sa1, NULL) == -1) {
        perror("[ERROR] Failed to configure SIGUSR1 handler");
        exit(EXIT_FAILURE);
    }

    struct sigaction sa2;
    memset(&sa2, 0, sizeof(sa2));
    sa2.sa_handler = handle_worker2_ready;
    sigemptyset(&sa2.sa_mask);
    sa2.sa_flags = 0;

    if (sigaction(SIGUSR2, &sa2, NULL) == -1) {
        perror("[ERROR] Failed to configure SIGUSR2 handler");
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

   pid_t pid2 = fork();
    if (pid2 == -1) {
        perror("[ERROR] Failed to fork Worker 2");
        return EXIT_FAILURE;
    }

    if (pid2 == 0) {
        printf("  [Worker 2] Started. PID: %d, PPID: %d\n", getpid(), getppid());
        printf("  [Worker 2] Initializing testing station...\n");
        sleep(2); // 2s simulation

        printf("  [Worker 2] Sending SIGUSR2 readiness signal to Supervisor...\n");
        if (kill(getppid(), SIGUSR2) == -1) {
            perror("  [Worker 2] Failed to send SIGUSR2");
            _exit(EXIT_FAILURE);
        }

        _exit(EXIT_SUCCESS);
    }

    // Supervisor waits for BOTH workers
    printf("[Supervisor] Awaiting readiness signals from both workers...\n");
    while (!worker1_ready || !worker2_ready) {
        pause();
    }
    printf("[Supervisor] Both workers reported ready! Starting production line.\n\n");

    // Reap child processes to prevent zombies
    int status;
    waitpid(pid1, &status, 0);
    waitpid(pid2, &status, 0);

    printf("[Supervisor] All workers shut down cleanly. Exiting.\n");
    return EXIT_SUCCESS;
}