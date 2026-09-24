#include "common.h"
#include <signal.h>

void run_worker2(int msqid) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("  [Worker 2] Started. PID: %d, PPID: %d\n", getpid(), getppid());
    printf("  [Worker 2] Opening FIFO for reading...\n");

    srand((unsigned int)(time(NULL) ^ (getpid() << 16)));

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

    fifo_packet_t packet;
    ssize_t bytes_read;
    long total_received = 0;
    long standard_count = 0;
    long defect_count = 0;

    // Read inspected products from FIFO until Worker 1 closes write end (EOF)
    while ((bytes_read = read(fifo_read_fd, &packet, sizeof(packet))) > 0) {
        if (total_received == 2) {
            take_break("[Worker 2]");
        }

        if (bytes_read != sizeof(packet)) {
            fprintf(stderr, "  [Worker 2] Warning: incomplete FIFO packet read.\n");
            continue;
        }

        total_received++;

        if (strcmp(packet.status, STATUS_STANDARD) == 0) {
            standard_count++;

            // Quality evaluation: score from 1 to 10
            int quality_score = (rand() % 10) + 1;

            printf("      [Worker 2] Testing item #%ld (serial #%d): Status [%s] -> Final Quality Score: [%d/10]\n",
                   total_received, packet.serial_number, packet.status, quality_score);

            // Prepare System V Message Queue packet
            mq_packet_t msg;
            msg.mtype = 1; // Standard message type (> 0)
            msg.serial_number = packet.serial_number;
            msg.quality_score = quality_score;

            // Send to Supervisor via message queue
            if (msgsnd(msqid, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
                perror("      [Worker 2] Failed to send message to queue");
            } else {
                printf("      [Worker 2] Dispatched item (serial #%d, score: %d) to Message Queue\n",
                       packet.serial_number, quality_score);
            }
        } else {
            defect_count++;
            printf("      [Worker 2] Skipping item #%ld (serial #%d): Status [%s] -> Discarded, no score assigned\n",
                   total_received, packet.serial_number, packet.status);
        }

        sleep(5);
    }

    if (bytes_read == -1) {
        perror("  [Worker 2] Error reading from FIFO");
        close(fifo_read_fd);
        exit(EXIT_FAILURE);
    }

    printf("  [Worker 2] Testing station finished. Tested %ld standard items, skipped %ld defects.\n",
           standard_count, defect_count);

    close(fifo_read_fd);
    exit(EXIT_SUCCESS);
}