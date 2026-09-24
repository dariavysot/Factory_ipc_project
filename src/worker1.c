#include "common.h"
#include <signal.h>

void run_worker1(int read_fd) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("  [Worker 1] Started. PID: %d, PPID: %d\n", getpid(), getppid());
    printf("  [Worker 1] Pipe read descriptor assigned: fd=%d\n", read_fd);

    // Seed RNG with unique PID and time to avoid identical random sequences
    srand((unsigned int)(time(NULL) ^ (getpid() << 16)));

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

    pipe_packet_t in_packet;
    ssize_t bytes_read;
    long processed_count = 0;
    long defect_count = 0;

    // Read streamed serial numbers from unnamed pipe
    while ((bytes_read = read(read_fd, &in_packet, sizeof(in_packet))) > 0) {
        if (bytes_read != sizeof(in_packet)) {
            fprintf(stderr, "  [Worker 1] Warning: incomplete packet read from pipe.\n");
            continue;
        }

        processed_count++;

        // Simulate 15% defect probability
        int roll = rand() % 100;
        int is_defect = (roll < DEFECT_PROBABILITY_PERCENT);

        fifo_packet_t out_packet;
        memset(&out_packet, 0, sizeof(out_packet));
        out_packet.serial_number = in_packet.serial_number;

        if (is_defect) {
            defect_count++;
            strncpy(out_packet.status, STATUS_DEFECT, sizeof(out_packet.status) - 1);
            printf("    [Worker 1] Item #%ld (serial #%d) inspected -> [DEFECT: Body issue]\n",
                   processed_count, in_packet.serial_number);
        } else {
            strncpy(out_packet.status, STATUS_STANDARD, sizeof(out_packet.status) - 1);
            printf("    [Worker 1] Item #%ld (serial #%d) inspected -> [STANDARD: Passed]\n",
                   processed_count, in_packet.serial_number);
        }

        // Send inspection result packet to Worker 2 via FIFO
        ssize_t bytes_written = write(fifo_write_fd, &out_packet, sizeof(out_packet));
        if (bytes_written != sizeof(out_packet)) {
            perror("  [Worker 1] Failed to write packet to FIFO");
            break;
        }

        sleep(8); // 80ms simulation delay per inspection
    }

    if (bytes_read == -1) {
        perror("  [Worker 1] Error reading from pipe");
        close(read_fd);
        close(fifo_write_fd);
        exit(EXIT_FAILURE);
    }

    printf("  [Worker 1] Completed. Total items: %ld (Standard: %ld, Defective: %ld). Closing stations.\n",
           processed_count, processed_count - defect_count, defect_count);

    close(read_fd);
    close(fifo_write_fd); // Closing write end signals EOF to Worker 2
    exit(EXIT_SUCCESS);
}