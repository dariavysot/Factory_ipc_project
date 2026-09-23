#include "common.h"
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>

/* Atomic readiness flags for both workers */
static volatile sig_atomic_t worker1_ready = 0;
static volatile sig_atomic_t worker2_ready = 0;

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

/* Worker 1 logic: takes the read descriptor */
static void run_worker1(int read_fd) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("  [Worker 1] Started. PID: %d, PPID: %d\n", getpid(), getppid());
    printf("  [Worker 1] Pipe read descriptor assigned: fd=%d\n", read_fd);
    sleep(1);

    printf("  [Worker 1] Sending SIGUSR1 readiness signal to Supervisor...\n");
    if (kill(getppid(), SIGUSR1) == -1) {
        perror("  [Worker 1] Failed to send SIGUSR1");
        close(read_fd);
        exit(EXIT_FAILURE);
    }

    pipe_packet_t packet;
    ssize_t bytes_read;
    long processed_count = 0;

    // Read streamed serial numbers until Supervisor closes the write end (EOF)
    while ((bytes_read = read(read_fd, &packet, sizeof(packet))) > 0) {
        if (bytes_read != sizeof(packet)) {
            fprintf(stderr, "  [Worker 1] Warning: incomplete packet read.\n");
            continue;
        }

        processed_count++;
        printf("    [Worker 1] Processed item #%ld: serial #%d received from pipe\n",
               processed_count, packet.serial_number);
        sleep(2); // 2s processing simulation per item
    }

    if (bytes_read == -1) {
        perror("  [Worker 1] Error reading from pipe");
        close(read_fd);
        exit(EXIT_FAILURE);
    }

    printf("  [Worker 1] Reached EOF on pipe. Total items received: %ld. Closing station.\n", processed_count);
    close(read_fd);
    exit(EXIT_SUCCESS);
}

/* Worker 2 logic */
static void run_worker2(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("  [Worker 2] Started. PID: %d, PPID: %d\n", getpid(), getppid());
    printf("  [Worker 2] Initializing testing station...\n");
    sleep(1);

    printf("  [Worker 2] Sending SIGUSR2 readiness signal to Supervisor...\n");
    if (kill(getppid(), SIGUSR2) == -1) {
        perror("  [Worker 2] Failed to send SIGUSR2");
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}

static void print_usage(FILE *stream, const char *prog_name) {
    fprintf(stream, "Usage: %s <total_items>\n", prog_name);
    fprintf(stream, "       %s -h | --help\n\n", prog_name);
    fprintf(stream, "Arguments:\n");
    fprintf(stream, "  <total_items>  Positive integer specifying total items to process\n\n");
    fprintf(stream, "Options:\n");
    fprintf(stream, "  -h, --help     Display this help message and exit\n");
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

    //Setup named pipe (FIFO) for communication between workers
    unlink(FIFO_PATH); // Clean up stale FIFO if exists
    if (mkfifo(FIFO_PATH, 0666) == -1) {
        perror("[ERROR] Failed to create FIFO");
        return EXIT_FAILURE;
    }
    printf("[Supervisor] Named pipe (FIFO) created at: %s\n", FIFO_PATH);

    //Create unnamed pipe before fork()
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        perror("[ERROR] Failed to create unnamed pipe");
        unlink(FIFO_PATH);
        return EXIT_FAILURE;
    }
    printf("[Supervisor] Unnamed pipe created successfully (read_fd=%d, write_fd=%d).\n", pipe_fd[0], pipe_fd[1]);

    /* Block signals to prevent race conditions during worker bootstrap */
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
        unlink(FIFO_PATH);
        return EXIT_FAILURE;
    }

    if (pid1 == 0) {
        sigprocmask(SIG_SETMASK, &orig_mask, NULL);
        close(pipe_fd[1]); 
        run_worker1(pipe_fd[0]);
    }

    // Fork Worker 2
    pid_t pid2 = fork();
    if (pid2 == -1) {
        perror("[ERROR] Failed to fork Worker 2");
        unlink(FIFO_PATH);
        return EXIT_FAILURE;
    }
    
    if (pid2 == 0) {
        sigprocmask(SIG_SETMASK, &orig_mask, NULL);
        // Worker 2 does not use this pipe; close both ends
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        run_worker2();
    }

    // Supervisor closes read end
    close(pipe_fd[0]);

    // Await ready signals
    printf("[Supervisor] Awaiting readiness signals from both workers...\n");
    while (!worker1_ready || !worker2_ready) {
        sigsuspend(&orig_mask);
    }
    sigprocmask(SIG_SETMASK, &orig_mask, NULL);

    printf("[Supervisor] Both workers reported ready! Starting production line.\n\n");

    // Seed random generator for serial numbers
    srand((unsigned int)time(NULL));

    // Stream serial numbers into the pipe
    printf("[Supervisor] Generating and dispatching %ld items...\n", total_items);
    for (long i = 0; i < total_items; i++) {
        pipe_packet_t packet;
        packet.serial_number = 10000 + (rand() % 90000); // 5-digit serial number

        ssize_t bytes_written = write(pipe_fd[1], &packet, sizeof(packet));
        if (bytes_written != sizeof(packet)) {
            perror("[ERROR] Failed to write item packet into pipe");
            break;
        }

        printf("  [Supervisor] Dispatched item [%ld/%ld]: serial #%d\n",
               i + 1, total_items, packet.serial_number);
    }

    // Close write end to signal EOF to reader
    close(pipe_fd[1]);
    printf("[Supervisor] All items dispatched. Pipe write end closed.\n\n");

    // Wait for children
    int status;
    waitpid(pid1, &status, 0);
    waitpid(pid2, &status, 0);

    // Clean up FIFO node from filesystem
    unlink(FIFO_PATH);
    printf("[Supervisor] Named pipe (FIFO) unlinked. Clean exit.\n");

    return EXIT_SUCCESS;
}