#include "common.h"
#include <signal.h>

void run_worker1(int read_fd) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("  [Worker 1] Started. PID: %d, PPID: %d\n", getpid(), getppid());
    printf("  [Worker 1] Pipe read descriptor assigned: fd=%d\n", read_fd);

    // Open FIFO for writing to Worker 2
    printf("  [Worker 1] Opening FIFO for writing...\n");
    int fifo_write_fd = open(FIFO_PATH, O_WRONLY);
    if (fifo_write_fd == -1) {
        perror("  [Worker 1] Failed to open FIFO for writing");
        close(read_fd);
        exit(EXIT_FAILURE);
    }
    printf("  [Worker 1] FIFO channel opened for writing (fd=%d).\n", fifo_write_fd);

    sleep(1);

    printf("  [Worker 1] Sending SIGUSR1 readiness signal to Supervisor...\n");
    if (kill(getppid(), SIGUSR1) == -1) {
        perror("  [Worker 1] Failed to send SIGUSR1");
        close(read_fd);
        close(fifo_write_fd);
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
        sleep(2);
    }

    if (bytes_read == -1) {
        perror("  [Worker 1] Error reading from pipe");
        close(read_fd);
        close(fifo_write_fd);
        exit(EXIT_FAILURE);
    }

    printf("  [Worker 1] Reached EOF on pipe. Total items received: %ld. Closing station.\n", processed_count);
    close(read_fd);
    close(fifo_write_fd); // Closing write end signals EOF to Worker 2
    exit(EXIT_SUCCESS);
}