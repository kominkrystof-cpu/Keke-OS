# 🐱 Keke OS

**A multi-language Linux-based operating system with custom kernel syscalls, a retro Windows XP-style GUI, and a built-in programming language.**

> **"Keke OS nevznikl u stolu psanim kodu. Vznikl v kvetnu 2026, kdy Keke zahral kombo Marshall na maximalni hlasitost..."**
>
> — Keke OS `origin` command

---

## Overview

Keke OS is a **Linux-based OS** with a custom init (`/init`) written in C++ that boots directly into a feature-rich shell and GUI. It's not a Linux distro in the traditional sense — it's a self-contained operating system experience where **everything is custom-built**.

| Feature | Implementation |
|---------|---------------|
| **Kernel** | Linux 7.1.2 (custom) + Keke syscall #472 |
| **Init (PID 1)** | C++ (`main.cpp`) — static binary |
| **Shell** | Keke Shell (C++) with command history, color themes |
| **GUI** | Windows XP Luna-style (pure framebuffer, software-rendered) |
| **Scripting** | Purr++ (built-in), Python, JavaScript, Shell |
| **Kernel Module** | `/dev/kekeos` — custom char device interface |

---

## Architecture

```
GRUB → Linux kernel → initramfs → /init (main.cpp)
                                    │
                                    ├─ Load kernel modules (bochs, psmouse, kekeos)
                                    ├─ Mount disk to /mnt
                                    ├─ Launch Keke Shell (built-in)
                                    │     ├─ Built-in commands (cd, ls, cat, calc, time...)
                                    │     ├─ Execute C programs (/usr/bin/keke_info)
                                    │     ├─ Run Python scripts (./script.py)
                                    │     ├─ Run JS scripts via QuickJS (./script.js)
                                    │     ├─ Run Shell scripts (./script.sh)
                                    │     └─ Launch GUI (Windows XP Luna style)
                                    └─ Handle login/password authentication
```

### Language Support

| Language | How | Status |
|----------|-----|--------|
| **C++** | Keke Shell + GUI (`main.cpp`, `gui.hpp`) | ✅ Built-in |
| **C** | Static binaries in `/usr/bin/` (keke_info, keketool) | ✅ Pre-compiled |
| **Python** | `/mnt/bin/python3` + shared libs on disk | ✅ Run via `./script.py` |
| **JavaScript** | QuickJS (`/mnt/bin/qjs`) | ✅ Run via `./script.js` |
| **Purr++** | Built-in interpreter with labels, goto, variables | ✅ Built-in |
| **Shell** | `/bin/sh` via kernel | ✅ Run via `./script.sh` |

### Custom Kernel Syscall (#472)

Keke OS adds a new Linux syscall `keke_cmd` at number 472 with these sub-commands:

| Command | Returns |
|---------|---------|
| `KEKE_CMD_GET_VERSION` | `{major, minor, patch, codename}` |
| `KEKE_CMD_GET_BOOT_TIME` | Uptime in seconds |
| `KEKE_CMD_HELLO` | Easter egg message |
| `KEKE_CMD_GET_STATS` | `{uptime, total_memory_kb, free_memory_kb, processes}` |
| `KEKE_CMD_RAISE_CAT` | ASCII art cat |
| `KEKE_CMD_SET_THEME` | (stub — not yet implemented) |

If the custom kernel isn't built yet, the same interface is available via **`/dev/kekeos`** (loadable kernel module).

---

## Quick Start

### Prerequisites

```bash
# Ubuntu/Debian
sudo apt install build-essential qemu-system-x86 qemu-utils grub-pc-bin \
                     parted zstd python3

# Fedora
sudo dnf groupinstall "Development Tools"
sudo dnf install qemu-system-x86 grub2 parted zstd python3
```

### Build

```bash
# 1. Clone
git clone https://github.com/yourusername/keke-os
cd "Absolute dogshit coding projects/Keke-OS"

# 2. Make the build script executable
chmod +x build.sh

# 3. Build everything (non-root parts)
./build.sh

# 4. Build root-requiring parts
sudo ./copy_kernel.sh
sudo ./build_initramfs.sh
sudo ./setup_disk.sh
```

### Boot in QEMU

```bash
# From disk image (recommended — has Python, JS, scripts)
qemu-system-x86_64 -drive file=keke-disk.img,format=raw -m 512 -serial stdio

# With GUI (framebuffer)
qemu-system-x86_64 -drive file=keke-disk.img,format=raw -m 512 -vga std -serial stdio

# From ISO
qemu-system-x86_64 -cdrom keke-os.iso -m 512 -serial stdio

# With KVM acceleration
qemu-system-x86_64 -drive file=keke-disk.img,format=raw -m 512 -enable-kvm -cpu host -serial stdio
```

---

## Project Structure

```
├── build.sh                    # One-command build script
├── build_all.sh                # Legacy build script
├── build_initramfs.sh          # Builds initramfs (needs root)
├── copy_kernel.sh              # Copies host kernel (needs root)
├── setup_disk.sh               # Creates disk image (needs root)
├── make_iso.sh                 # Creates bootable ISO
├── grub.cfg                    # GRUB boot configuration
├── CHANGELOG.md                # Version history
├── LICENSE_COMPLIANCE.md       # GPL compliance for kernel changes
│
├── keke-src/
│   ├── main.cpp                # Init binary (PID 1) — Keke Shell
│   ├── gui.hpp                 # Windows XP-style GUI framework
│   ├── kekeos.h                # Userspace API for custom syscall
│   ├── keke.h                  # Struct definitions
│   ├── keke_info.c             # C system info tool
│   ├── keketool.c              # C CLI tool for syscalls
│   ├── Makefile                # Builds C++ + C programs
│   └── kekeos-mod/             # Kernel module source
│       ├── kekeos_dev.c        # /dev/kekeos character device
│       ├── keke.h              # Shared UAPI header
│       └── Makefile
│
├── linux-7.1.2/                # Custom Linux kernel source (GPL)
│   └── kernel/keke.c           # + Keke syscall #472 implementation
│
├── disk_content/               # Files placed on the disk image
│   ├── babicka.txt             # Easter egg
│   └── scripts/                # Example scripts
│       ├── sysinfo.py          # Python system info
│       ├── demo.js             # JavaScript demo
│       └── info.sh             # Shell script
│
├── build/                      # Build artifacts
│   ├── init                    # C++ init binary
│   ├── keke_info               # C program
│   ├── keketool                # C CLI tool
│   ├── kekeos-mod.ko           # Kernel module
│   ├── qjs                     # QuickJS interpreter
│   ├── vmlinuz                 # Linux kernel
│   └── keke-initramfs.cpio.gz  # Initramfs archive
│
├── kernel.c                    # (Reference only — old bare-metal kernel)
└── OPENBSD_SWITCH_ANALYSIS.md  # (Analysis doc — not part of build)
```

---

## Commands

| Command | Description |
|---------|-------------|
| `help` | List available commands |
| `ver` | Show OS version |
| `cls` | Clear screen |
| `time` | Show current time (RTC) |
| `calc` | Interactive calculator |
| `cd <dir>` | Change directory |
| `ls` | List files |
| `mkdir <dir>` | Create directory |
| `rm <file>` | Remove file |
| `touch <file>` | Create empty file |
| `cat <file>` | Read file (or easter egg if no arg) |
| `kpm <subcmd>` | Keke Package Manager |
| `gui` | Launch XP-style GUI |
| `color <name>` | Change terminal colors |
| `cursor <arrow\|paw>` | Change cursor style |
| `keke_info` | System info via custom syscall |
| `keketool <cmd>` | Keke CLI tool |
| `./script.py` | Run Python script |
| `./script.js` | Run JavaScript script |
| `./script.sh` | Run Shell script |
| `./script` | Run Purr++ script |
| `origin` | Keke OS origin story |
| `windows` | Bill Gates story |
| `reboot` | Soft-reboot the shell |
| `exit` | Exit to login |

---

## License

- **Linux kernel** (`linux-7.1.2/`): GNU General Public License v2
- **Keke OS userspace** (`keke-src/`, `build_initramfs.sh`, etc.): Proprietary / All Rights Reserved
- **Keke kernel module** (`keke-src/kekeos-mod/`): GNU General Public License v2
- **QuickJS** (`build/qjs`): MIT License

See `LICENSE_COMPLIANCE.md` for details on our GPL compliance with modified kernel source.

---

## Screenshots

*(coming soon)*

---

## Credits

- **Keke** — Architecture, C++ shell, GUI, Purr++ interpreter
- **Fabrice Bellard** — QuickJS, QEMU
- **Linus Torvalds** — Linux kernel
- **GRUB team** — Bootloader

---

*"Kdyz Bill Gates v roce 1985 zakladal Windows, sedel v kancelari a dival se skrz realne sklenene okno..."*
