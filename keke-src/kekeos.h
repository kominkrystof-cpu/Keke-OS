#ifndef KEKE_USERSPACE_H
#define KEKE_USERSPACE_H

#include <stdint.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "keke.h"

#ifndef SYS_keke_cmd
#define SYS_keke_cmd 472
#endif

static inline int keke_cmd_syscall(unsigned int cmd, unsigned long arg, void *out)
{
    return syscall(SYS_keke_cmd, cmd, arg, out);
}

static inline int keke_cmd_devctl(unsigned int cmd, void *out)
{
    int fd = open("/dev/kekeos", O_RDWR);
    if (fd < 0) return -1;
    int ret = ioctl(fd, cmd, out);
    close(fd);
    return ret;
}

static inline int keke_get_version(struct keke_version *ver)
{
    int ret = keke_cmd_syscall(KEKE_CMD_GET_VERSION, 0, ver);
    if (ret < 0) ret = keke_cmd_devctl(KEKE_CMD_GET_VERSION, ver);
    return ret;
}

static inline int keke_get_uptime(unsigned long *uptime)
{
    int ret = keke_cmd_syscall(KEKE_CMD_GET_BOOT_TIME, 0, uptime);
    if (ret < 0) ret = keke_cmd_devctl(KEKE_CMD_GET_BOOT_TIME, uptime);
    return ret;
}

static inline int keke_get_stats(struct keke_stats *stats)
{
    int ret = keke_cmd_syscall(KEKE_CMD_GET_STATS, 0, stats);
    if (ret < 0) ret = keke_cmd_devctl(KEKE_CMD_GET_STATS, stats);
    return ret;
}

static inline int keke_hello(void)
{
    char buf[64] = {0};
    int ret = keke_cmd_syscall(KEKE_CMD_HELLO, 0, buf);
    if (ret < 0) ret = keke_cmd_devctl(KEKE_CMD_HELLO, buf);
    if (ret == 0) { (void)write(STDOUT_FILENO, buf, sizeof(buf)); }
    return ret;
}

static inline int keke_raise_cat(void)
{
    char buf[64] = {0};
    int ret = keke_cmd_syscall(KEKE_CMD_RAISE_CAT, 0, buf);
    if (ret < 0) ret = keke_cmd_devctl(KEKE_CMD_RAISE_CAT, buf);
    if (ret == 0) { (void)write(STDOUT_FILENO, buf, sizeof(buf)); }
    return ret;
}

#endif
