#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/sysinfo.h>
#include <linux/timekeeping.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include "keke.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Keke OS");
MODULE_DESCRIPTION("Keke OS custom character device - immediate kernel interface");
MODULE_VERSION("2.8.0");

#define KEKE_VERSION_MAJOR 2
#define KEKE_VERSION_MINOR 8
#define KEKE_VERSION_PATCH 0
#define KEKE_CODENAME "MSYS"

static unsigned long boot_time_sec;

static long keke_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    void __user *user_ptr = (void __user *)arg;

    switch (cmd) {
    case KEKE_CMD_GET_VERSION: {
        struct keke_version ver;
        memset(&ver, 0, sizeof(ver));
        ver.major = KEKE_VERSION_MAJOR;
        ver.minor = KEKE_VERSION_MINOR;
        ver.patch = KEKE_VERSION_PATCH;
        strscpy(ver.codename, KEKE_CODENAME, sizeof(ver.codename));
        if (copy_to_user(user_ptr, &ver, sizeof(ver)))
            return -EFAULT;
        return 0;
    }
    case KEKE_CMD_GET_BOOT_TIME: {
        unsigned long now = ktime_get_seconds();
        unsigned long uptime = now - boot_time_sec;
        if (copy_to_user(user_ptr, &uptime, sizeof(uptime)))
            return -EFAULT;
        return 0;
    }
    case KEKE_CMD_HELLO: {
        char msg[64];
        memset(msg, 0, sizeof(msg));
        strscpy(msg, "Keke OS says: Meow! Rock'n'roll!\n", sizeof(msg));
        if (copy_to_user(user_ptr, msg, sizeof(msg)))
            return -EFAULT;
        return 0;
    }
    case KEKE_CMD_GET_STATS: {
        struct sysinfo si;
        si_meminfo(&si);
        struct keke_stats stats;
        memset(&stats, 0, sizeof(stats));
        stats.uptime_seconds = ktime_get_seconds() - boot_time_sec;
        stats.total_memory_kb = (si.totalram * si.mem_unit) / 1024;
        stats.free_memory_kb = (si.freeram * si.mem_unit) / 1024;
        stats.processes = 0;
        if (copy_to_user(user_ptr, &stats, sizeof(stats)))
            return -EFAULT;
        return 0;
    }
    case KEKE_CMD_RAISE_CAT: {
        char cat[] = " /\\_/\\\n( o.o )\n > ^ <\n";
        pr_info("Keke OS: %s", cat);
        if (copy_to_user(user_ptr, cat, sizeof(cat)))
            return -EFAULT;
        return 0;
    }
    default:
        return -EINVAL;
    }
}

static struct file_operations keke_fops = {
    .owner          = THIS_MODULE,
    .unlocked_ioctl = keke_ioctl,
};

static struct miscdevice keke_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = "kekeos",
    .fops  = &keke_fops,
};

static int __init keke_mod_init(void)
{
    boot_time_sec = ktime_get_seconds();

    int ret = misc_register(&keke_misc);
    if (ret < 0) {
        pr_err("Keke OS: Failed to register misc device\n");
        return ret;
    }

    pr_info("Keke OS module loaded, /dev/kekeos ready\n");
    return 0;
}

static void __exit keke_mod_exit(void)
{
    misc_deregister(&keke_misc);
    pr_info("Keke OS module unloaded\n");
}

module_init(keke_mod_init);
module_exit(keke_mod_exit);
