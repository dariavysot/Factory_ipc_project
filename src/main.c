#include "common.h"
#include <signal.h>
#include <sys/wait.h>

/* Atomic readiness flag for both workers */
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


/* Unified signal handler for both worker readiness signals */
static void handle_worker_ready(int sig) {
    if (sig == SIGUSR1) {
        worker1_ready = 1;
    } else if (sig == SIGUSR2) {
        worker2_ready = 1;
    }
}

/* Register readiness signals using a single sigaction configuration */
static void setup_supervisor_signals(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_worker_ready;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGUSR1, &sa, NULL) == -1 || sigaction(SIGUSR2, &sa, NULL) == -1) {
        perror("[ERROR] Failed to configure worker signal handlers");
        exit(EXIT_FAILURE);
    }
}

/* Logic executed by Worker 1 process */
static void run_worker1(void) {
    printf("  [Worker 1] Started. PID: %d, PPID: %d\n", getpid(), getppid());
    printf("  [Worker 1] Initializing station...\n");
    sleep(2);

    printf("  [Worker 1] Sending SIGUSR1 readiness signal to Supervisor...\n");
    if (kill(getppid(), SIGUSR1) == -1) {
        perror("  [Worker 1] Failed to send SIGUSR1");
        _exit(EXIT_FAILURE);
    }
    _exit(EXIT_SUCCESS);
}

/* Logic executed by Worker 2 process */
static void run_worker2(void) {
    printf("  [Worker 2] Started. PID: %d, PPID: %d\n", getpid(), getppid());
    printf("  [Worker 2] Initializing testing station...\n");
    sleep(2);

    printf("  [Worker 2] Sending SIGUSR2 readiness signal to Supervisor...\n");
    if (kill(getppid(), SIGUSR2) == -1) {
        perror("  [Worker 2] Failed to send SIGUSR2");
        _exit(EXIT_FAILURE);
    }
    _exit(EXIT_SUCCESS);
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

    /* Block signals temporarily to prevent race conditions during child startup */
    sigset_t block_mask, orig_mask;
    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGUSR1);
    sigaddset(&block_mask, SIGUSR2);
    sigprocmask(SIG_BLOCK, &block_mask, &orig_mask);

    printf("[Supervisor] Process started. PID: %d\n", getpid());
    printf("[Supervisor] Target items count to produce: %ld\n\n", total_items);

    // Fork Worker 1
    pid_t pid1 = fork();
    if (pid1 == -1) {
        perror("[ERROR] Failed to fork Worker 1");
        return EXIT_FAILURE;
    }

    if (pid1 == 0) {
        sigprocmask(SIG_SETMASK, &orig_mask, NULL); // Restore original mask in child
        run_worker1();
    }
    
    // Fork Worker 2
    pid_t pid2 = fork();
    if (pid2 == -1) {
        perror("[ERROR] Failed to fork Worker 2");
        return EXIT_FAILURE;
    }

    if (pid2 == 0) {
        sigprocmask(SIG_SETMASK, &orig_mask, NULL); // Restore original mask in child
        run_worker2();
    }

    // Safe waiting loop using sigsuspend (atomic unblock and wait)
    printf("[Supervisor] Awaiting readiness signals from both workers...\n");
    while (!worker1_ready || !worker2_ready) {
        sigsuspend(&orig_mask);
    }
    sigprocmask(SIG_SETMASK, &orig_mask, NULL); // Restore mask after signals are received
    printf("[Supervisor] Both workers reported ready! Starting production line.\n\n");

    // Wait for children to finish
    int status;
    waitpid(pid1, &status, 0);
    waitpid(pid2, &status, 0);

    printf("[Supervisor] All workers shut down cleanly. Exiting.\n");
    return EXIT_SUCCESS;
}