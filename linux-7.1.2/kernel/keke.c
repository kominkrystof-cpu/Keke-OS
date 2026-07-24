#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/timekeeping.h>
#include <uapi/linux/keke.h>

#define KEKE_VERSION_MAJOR 2
#define KEKE_VERSION_MINOR 7
#define KEKE_VERSION_PATCH 5
#define KEKE_CODENAME "MSYS"

static unsigned long boot_time_sec;

static int __init keke_init_boot_time(void)
{
    boot_time_sec = ktime_get_seconds();
    return 0;
}
early_initcall(keke_init_boot_time);

SYSCALL_DEFINE3(keke_cmd, unsigned int, cmd, unsigned long, arg,
        unsigned long __user *, out)
{
    switch (cmd) {
    case KEKE_CMD_GET_VERSION: {
        struct keke_version ver;
        memset(&ver, 0, sizeof(ver));
        ver.major = KEKE_VERSION_MAJOR;
        ver.minor = KEKE_VERSION_MINOR;
        ver.patch = KEKE_VERSION_PATCH;
        strscpy(ver.codename, KEKE_CODENAME, sizeof(ver.codename));
        if (copy_to_user(out, &ver, sizeof(ver)))
            return -EFAULT;
        return 0;
    }
    case KEKE_CMD_GET_BOOT_TIME: {
        unsigned long now = ktime_get_seconds();
        unsigned long uptime = now - boot_time_sec;
        if (copy_to_user(out, &uptime, sizeof(uptime)))
            return -EFAULT;
        return 0;
    }
    case KEKE_CMD_HELLO: {
        char msg[64];
        memset(msg, 0, sizeof(msg));
        strscpy(msg, "Keke OS says: Meow! Rock'n'roll!\n", sizeof(msg));
        if (copy_to_user(out, msg, sizeof(msg)))
            return -EFAULT;
        return 0;
    }
    case KEKE_CMD_GET_STATS: {
        unsigned long freeram = nr_free_pages() * (PAGE_SIZE / 1024);
        unsigned long totalram = totalram_pages() * (PAGE_SIZE / 1024);
        struct keke_stats stats;
        memset(&stats, 0, sizeof(stats));
        stats.uptime_seconds = ktime_get_seconds() - boot_time_sec;
        stats.total_memory_kb = totalram;
        stats.free_memory_kb = freeram;
        stats.processes = nr_threads;
        if (copy_to_user(out, &stats, sizeof(stats)))
            return -EFAULT;
        return 0;
    }
    case KEKE_CMD_SET_THEME:
        return -EOPNOTSUPP;
    case KEKE_CMD_RAISE_CAT:
        pr_info("Keke OS: /\\_/\\  ( >.< )  > ^ <\n");
        return 0;
    default:
        return -EINVAL;
    }
}
