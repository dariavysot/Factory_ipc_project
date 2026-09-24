#include "common.h"
#include <signal.h>

void run_worker2(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("  [Worker 2] Started. PID: %d, PPID: %d\n", getpid(), getppid());
    printf("  [Worker 2] Opening FIFO for reading...\n");

    // Open FIFO for reading from Worker 1
    int fifo_read_fd = open(FIFO_PATH, O_RDONLY);
    if (fifo_read_fd == -1) {
        perror("  [Worker 2] Failed to open FIFO for reading");
        exit(EXIT_FAILURE);
    }
    printf("  [Worker 2] FIFO channel opened for reading (fd=%d).\n", fifo_read_fd);

    sleep(1);

    printf("  [Worker 2] Sending SIGUSR2 readiness signal to Supervisor...\n");
    if (kill(getppid(), SIGUSR2) == -1) {
        perror("  [Worker 2] Failed to send SIGUSR2");
        close(fifo_read_fd);
        exit(EXIT_FAILURE);
    }

    // Temporary wait loop until Worker 1 sends EOF
    char dummy_buf[128];
    while (read(fifo_read_fd, dummy_buf, sizeof(dummy_buf)) > 0) {
        // Discard until Step 2-3 implementation
    }

    printf("  [Worker 2] FIFO reached EOF. Closing reader station.\n");
    close(fifo_read_fd);
    exit(EXIT_SUCCESS);
}