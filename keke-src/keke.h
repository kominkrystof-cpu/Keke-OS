#ifndef _UAPI_LINUX_KEKE_H
#define _UAPI_LINUX_KEKE_H

#define KEKE_VERSION_STR "2.8.0"

#define KEKE_CMD_GET_VERSION   0
#define KEKE_CMD_GET_BOOT_TIME 1
#define KEKE_CMD_HELLO         2
#define KEKE_CMD_GET_STATS     3
#define KEKE_CMD_SET_THEME     4
#define KEKE_CMD_RAISE_CAT     5

struct keke_version {
    int major;
    int minor;
    int patch;
    char codename[64];
};

struct keke_stats {
    unsigned long uptime_seconds;
    unsigned long total_memory_kb;
    unsigned long free_memory_kb;
    unsigned long processes;
};

#endif
