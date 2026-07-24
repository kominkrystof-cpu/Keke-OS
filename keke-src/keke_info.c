#include <stdio.h>
#include "kekeos.h"

int main(int argc, char **argv)
{
    printf("Keke OS System Information\n");
    printf("=========================\n\n");

    struct keke_version ver;
    if (keke_get_version(&ver) == 0) {
        printf("Version: %d.%d.%d (%s)\n", ver.major, ver.minor, ver.patch, ver.codename);
    } else {
        printf("Version: (kernel module not loaded)\n");
    }

    unsigned long uptime;
    if (keke_get_uptime(&uptime) == 0) {
        unsigned long days = uptime / 86400;
        unsigned long hours = (uptime % 86400) / 3600;
        unsigned long mins = (uptime % 3600) / 60;
        unsigned long secs = uptime % 60;
        printf("Uptime: %lud %luh %lum %lus\n", days, hours, mins, secs);
    }

    struct keke_stats stats;
    if (keke_get_stats(&stats) == 0) {
        printf("Memory: %lukB total, %lukB free\n",
               stats.total_memory_kb, stats.free_memory_kb);
        printf("Processes: %lu\n", stats.processes);
    }

    printf("\n");
    keke_hello();
    keke_raise_cat();

    return 0;
}
