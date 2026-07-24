#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kekeos.h"

static void print_usage(void)
{
    printf("Usage: keketool <command>\n");
    printf("Commands:\n");
    printf("  info       - Show system info\n");
    printf("  version    - Show Keke OS version\n");
    printf("  uptime     - Show system uptime\n");
    printf("  stats      - Show memory/process stats\n");
    printf("  cat        - Raise a cat\n");
    printf("  hello      - Keke OS says hello\n");
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        print_usage();
        return 1;
    }

    if (strcmp(argv[1], "info") == 0) {
        struct keke_version ver;
        if (keke_get_version(&ver) == 0)
            printf("Keke OS %d.%d.%d (%s)\n", ver.major, ver.minor, ver.patch, ver.codename);

        struct keke_stats stats;
        if (keke_get_stats(&stats) == 0)
            printf("Uptime: %lus | Mem: %lukB free / %lukB total | Procs: %lu\n",
                   stats.uptime_seconds, stats.free_memory_kb,
                   stats.total_memory_kb, stats.processes);
    }
    else if (strcmp(argv[1], "version") == 0) {
        struct keke_version ver;
        if (keke_get_version(&ver) == 0)
            printf("%d.%d.%d %s\n", ver.major, ver.minor, ver.patch, ver.codename);
        else
            printf("Keke OS module not loaded\n");
    }
    else if (strcmp(argv[1], "uptime") == 0) {
        unsigned long uptime;
        if (keke_get_uptime(&uptime) == 0)
            printf("%lu seconds\n", uptime);
        else
            printf("Keke OS module not loaded\n");
    }
    else if (strcmp(argv[1], "stats") == 0) {
        struct keke_stats stats;
        if (keke_get_stats(&stats) == 0)
            printf("Memory: %lukB / %lukB | Processes: %lu\n",
                   stats.free_memory_kb, stats.total_memory_kb, stats.processes);
        else
            printf("Keke OS module not loaded\n");
    }
    else if (strcmp(argv[1], "cat") == 0) {
        keke_raise_cat();
    }
    else if (strcmp(argv[1], "hello") == 0) {
        keke_hello();
    }
    else {
        printf("Unknown command: %s\n", argv[1]);
        print_usage();
        return 1;
    }

    return 0;
}
