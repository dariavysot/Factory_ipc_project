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

static void print_usage(FILE *stream, const char *prog_name) {
    fprintf(stream, "Usage: %s <total_items>\n", prog_name);
    fprintf(stream, "       %s -h | --help\n\n", prog_name);
    fprintf(stream, "Arguments:\n");
    fprintf(stream, "  <total_items>  Positive integer specifying total items to process\n\n");
    fprintf(stream, "Options:\n");
    fprintf(stream, "  -h, --help     Display this help message and exit\n");
}

/* Initializes and purges all IPC primitives required by the factory system */
static void init_ipc_resources(supervisor_ipc_t *ipc) {
    // Setup named pipe (FIFO)
    unlink(FIFO_PATH); // Remove stale FIFO if left over
    if (mkfifo(FIFO_PATH, 0666) == -1) {
        perror("[ERROR] Failed to create FIFO");
        exit(EXIT_FAILURE);
    }
    printf("[Supervisor] Named pipe (FIFO) created at: %s\n", FIFO_PATH);

    // Setup System V Message Queue
    key_t msg_key = ftok(".", PROJECT_ID);
    if (msg_key == -1) {
        perror("[ERROR] Failed to generate message queue key via ftok");
        unlink(FIFO_PATH);
        exit(EXIT_FAILURE);
    }

    ipc->msqid = msgget(msg_key, IPC_CREAT | 0666);
    if (ipc->msqid == -1) {
        perror("[ERROR] Failed to create message queue");
        unlink(FIFO_PATH);
        exit(EXIT_FAILURE);
    }

    // Drain leftover messages from any previously killed runs
    mq_packet_t stale_drain;
    while (msgrcv(ipc->msqid, &stale_drain, sizeof(stale_drain) - sizeof(long), 0, IPC_NOWAIT) != -1) {
        /* Purge */
    }
    printf("[Supervisor] Message queue initialized and purged (msqid=%d, key=0x%x).\n", ipc->msqid, msg_key);

    // Setup POSIX Named Semaphore
    sem_unlink(SEM_NAME); // Clean up stale semaphore if exists
    ipc->break_sem = sem_open(SEM_NAME, O_CREAT | O_EXCL, 0666, 1);
    if (ipc->break_sem == SEM_FAILED) {
        perror("[ERROR] Failed to create break semaphore");
        msgctl(ipc->msqid, IPC_RMID, NULL);
        unlink(FIFO_PATH);
        exit(EXIT_FAILURE);
    }
    printf("[Supervisor] Break synchronization semaphore initialized: %s (value=1).\n", SEM_NAME);

    // Create Unnamed Pipe before fork()
    if (pipe(ipc->pipe_fd) == -1) {
        perror("[ERROR] Failed to create unnamed pipe");
        sem_close(ipc->break_sem);
        sem_unlink(SEM_NAME);
        msgctl(ipc->msqid, IPC_RMID, NULL);
        unlink(FIFO_PATH);
        exit(EXIT_FAILURE);
    }
    printf("[Supervisor] Unnamed pipe created successfully (read_fd=%d, write_fd=%d).\n",
           ipc->pipe_fd[0], ipc->pipe_fd[1]);
}

/**
 * Deallocates and unlinks all active IPC resources during graceful shutdown or abort.
 */
static void cleanup_ipc_resources(supervisor_ipc_t *ipc) {
    if (ipc->break_sem != SEM_FAILED && ipc->break_sem != NULL) {
        sem_close(ipc->break_sem);
    }
    if (sem_unlink(SEM_NAME) == -1 && errno != ENOENT) {
        perror("[ERROR] Failed to unlink semaphore");
    } else {
        printf("[Supervisor] Break semaphore unlinked successfully.\n");
    }

    if (ipc->msqid != -1) {
        if (msgctl(ipc->msqid, IPC_RMID, NULL) == -1 && errno != EINVAL) {
            perror("[ERROR] Failed to remove message queue");
        } else {
            printf("[Supervisor] Message queue deallocated successfully.\n");
        }
    }

    unlink(FIFO_PATH);
    printf("[Supervisor] Named pipe (FIFO) unlinked. Clean exit.\n");
}

/**
 * Suspends execution until both workers signal readiness (SIGUSR1, SIGUSR2).
 */
static void await_workers_readiness(sigset_t *orig_mask) {
    printf("[Supervisor] Awaiting readiness signals from both workers...\n");
    while (!worker1_ready || !worker2_ready) {
        sigsuspend(orig_mask);
    }
    printf("[Supervisor] Both workers reported ready! Starting production line.\n\n");
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

    // Initialize all IPC channels (FIFO, Message Queue, Semaphore, Pipe)
    supervisor_ipc_t ipc;
    init_ipc_resources(&ipc);

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
        cleanup_ipc_resources(&ipc);
        return EXIT_FAILURE;
    }

    if (pid1 == 0) {
        sigprocmask(SIG_SETMASK, &orig_mask, NULL);
        close(ipc.pipe_fd[1]);
        run_worker1(ipc.pipe_fd[0]);
    }


    // Fork Worker 2
    pid_t pid2 = fork();
    if (pid2 == -1) {
        perror("[ERROR] Failed to fork Worker 2");
        cleanup_ipc_resources(&ipc);
        return EXIT_FAILURE;
    }
   
    if (pid2 == 0) {
        sigprocmask(SIG_SETMASK, &orig_mask, NULL);
        // Worker 2 does not use this pipe; close both ends
        close(ipc.pipe_fd[0]);
        close(ipc.pipe_fd[1]);
        run_worker2(ipc.msqid);
    }


    // Supervisor closes read end of pipe
    close(ipc.pipe_fd[0]);

    // Await readiness handshake from both workers
    await_workers_readiness(&orig_mask);
    sigprocmask(SIG_SETMASK, &orig_mask, NULL);

    // Seed random generator for serial numbers
    srand((unsigned int)time(NULL));


    // Stream serial numbers into unnamed pipe
    printf("[Supervisor] Generating and dispatching %ld items...\n", total_items);
    for (long i = 0; i < total_items; i++) {
        pipe_packet_t packet;
        packet.serial_number = 10000 + (rand() % 90000); // 5-digit serial number

        ssize_t bytes_written = write(ipc.pipe_fd[1], &packet, sizeof(packet));
        if (bytes_written != sizeof(packet)) {
            perror("[ERROR] Failed to write item packet into pipe");
            break;
        }


        printf("  [Supervisor] Dispatched item [%ld/%ld]: serial #%d\n",
               i + 1, total_items, packet.serial_number);
    }


    // Close pipe to signal EOF to Worker 1
    close(ipc.pipe_fd[1]);
    printf("[Supervisor] All items dispatched. Pipe write end closed.\n\n");


    // Wait for worker termination
    int status;
    waitpid(pid1, &status, 0);
    waitpid(pid2, &status, 0);


    // Read and print final quality test results from Message Queue
    printf("\n================ [SUPERVISOR QUALITY REPORT] ================\n");
    mq_packet_t result_msg;
    long passed_count = 0;
    double total_score = 0.0;

    while (msgrcv(ipc.msqid, &result_msg, sizeof(result_msg) - sizeof(long), 0, IPC_NOWAIT) != -1) {
        passed_count++;
        total_score += result_msg.quality_score;
        printf("  -> Verified item #%ld | Serial: %d | Quality Score: %d/10\n",
               passed_count, result_msg.serial_number, result_msg.quality_score);
    }


    if (errno != ENOMSG && errno != 0) {
        perror("[ERROR] Error reading from message queue");
    }


    printf("-------------------------------------------------------------\n");
    printf("  Total items dispatched: %ld\n", total_items);
    printf("  Items passed to final test: %ld\n", passed_count);
    printf("  Items rejected as defect: %ld\n", total_items - passed_count);
    if (passed_count > 0) {
        printf("  Average quality score: %.2f / 10\n", total_score / (double)passed_count);
    }
    printf("=============================================================\n\n");

    // Deallocate and clean up all IPC resources
    cleanup_ipc_resources(&ipc);

    return EXIT_SUCCESS;
}