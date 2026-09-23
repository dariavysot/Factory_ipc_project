#include "common.h"

static void print_usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s <total_items>\n", prog_name);
    fprintf(stderr, "  <total_items> : positive integer greater than 0\n");
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "[ERROR] Missing required argument.\n");
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    char *endptr = NULL;
    errno = 0;
    long total_items = strtol(argv[1], &endptr, 10);

    if (errno != 0 || *endptr != '\0' || total_items <= 0) {
        fprintf(stderr, "[ERROR] Invalid number of items: '%s'. Must be a positive integer.\n", argv[1]);
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    printf("[Supervisor] Process started. PID: %d\n", getpid());
    printf("[Supervisor] Target items count to produce: %ld\n", total_items);

    return EXIT_SUCCESS;
}