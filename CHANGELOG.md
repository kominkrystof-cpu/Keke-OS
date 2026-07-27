# Changelog

## v2.7.8 — Shell Tooling + Networking + KPM (2026-07-27)

### Shell Tooling
- **Pipe support**: `cmd1 | cmd2` — pipes stdout of cmd1 to stdin of cmd2 via `pipe()` + `fork()`
- **Output redirection**: `cmd > file` (overwrite), `cmd >> file` (append)
- **Background jobs**: `cmd &` — runs command in background, returns prompt immediately
- **`jobs`** — lists running background jobs with PID and status
- **`fg`** — brings last background job to foreground
- **`bg`** — confirms all background jobs are running
- **`cp <src> <dst>`** — copy files (delegates to `/bin/sh` cp)
- **`mv <src> <dst>`** — move/rename files
- **`chmod <mode> <file>`** — change file permissions
- **`find [path]`** — find files recursively (default: current directory)
- **`grep <pattern> [files...]`** — search text patterns

### Networking + KPM (Phase 2)
- Real TCP socket support via Linux kernel (`socket()`, `connect()`, `send()`, `recv()`)
- **`net <url>`** — raw HTTP GET request for debugging
- **`kpm repo <url>`** — set package repository URL
- **`kpm list`** — fetches `packages.json` from repo, shows available packages
- **`kpm install <pkg>`** — downloads `.pkg` file from repo over HTTP
- **`kpm update`** — refreshes package list from remote
- **`kpm repo`** — shows current repository URL
- QEMU user-mode networking: `-netdev user,id=net0 -device e1000,netdev=net0`

### Previous versions
- v2.7.7: Networking + Real KPM
- v2.7.6: Git hygiene + GUI architecture review
- v2.7.5: Multi-language support, custom kernel syscalls

### Architecture Change
- **Option B implemented**: Custom Linux kernel with Keke-specific syscalls
- **Multi-language support**: moved from C++-only to C, Python, JavaScript, Purr++, Shell
- **Keke Shell kept as-is** — same UI, same commands, just smarter backend

### Kernel (linux-7.1.2/)
- **New syscall `keke_cmd` (#472)** — 6 sub-commands:
  - `KEKE_CMD_GET_VERSION` — returns `{major, minor, patch, codename}`
  - `KEKE_CMD_GET_BOOT_TIME` — returns uptime in seconds
  - `KEKE_CMD_HELLO` — easter egg ("Keke OS says: Meow! Rock'n'roll!")
  - `KEKE_CMD_GET_STATS` — returns `{uptime, total_memory_kb, free_memory_kb, processes}`
  - `KEKE_CMD_RAISE_CAT` — ASCII cat in dmesg
  - `KEKE_CMD_SET_THEME` — stub (not yet implemented)
- **Syscall table** updated: `arch/x86/entry/syscalls/syscall_64.tbl` line 472
- **Build infra**: `kernel/keke.c` added to `kernel/Makefile`

### Kernel Module (keke-src/kekeos-mod/)
- **`/dev/kekeos` character device** — same interface as syscall but works immediately without kernel rebuild
- Loaded at boot by `main.cpp` (`loadKernelModule("/lib/modules/kekeos-mod.ko")`)
- Uses `miscdevice` framework — auto-creates `/dev/kekeos`

### C Programs (keke-src/)
- **`keke_info.c`** — prints OS version, uptime, memory stats via custom syscall
- **`keketool.c`** — CLI tool with subcommands: `info`, `version`, `uptime`, `stats`, `cat`, `hello`
- **`kekeos.h`** — userspace API header wrapping both syscall (#472) and `/dev/kekeos` ioctl
- **`keke.h`** — struct definitions shared with kernel UAPI

### Interpreters
- **QuickJS** (`build/qjs`, 2.6MB static) — JavaScript engine, placed on disk at `/mnt/bin/qjs`
- **Python 3** — copied from host + shared libs to `/mnt/bin/python3`
- Both invoked automatically by `executeFile()` based on `.py`/`.js` extension detection

### Shell (main.cpp) Changes
- **`executeFile()`** now detects `.py` → `/mnt/bin/python3`, `.js` → `/mnt/bin/qjs`, `.sh` → `/bin/sh`
- **External command fallback** — unknown commands now searched in `./`, `/mnt/bin/`, `/usr/bin/`, `/bin/` before showing "command not found"
- **Boot module order**: devtmpfs → psmouse.ko → kekeos-mod.ko → disk mount
- **`ver` command** updated to show "Custom Linux + Keke syscalls" and language list
- **`help`** updated with new command names

### Build System
- **`build_all.sh`** — one-command build (kernel module → C++/C binaries → initramfs → disk image)
- **`build_initramfs.sh`** — now includes `kekeos-mod.ko`, `keke_info`, `keketool` in initramfs
- **`setup_disk.sh`** — now populates disk with Python, QuickJS, C programs, example scripts
- **`keke-src/Makefile`** — builds both C++ init and C programs

### Scripts (disk_content/)
- `scripts/sysinfo.py` — Python system info reader
- `scripts/demo.js` — JavaScript (QuickJS) demo (fibonacci, arrays, date)
- `scripts/info.sh` — Shell script demo

### Removed / Dead Code
- `kernel.c` unchanged (old bare-metal reference, not used in boot)
- `OPENBSD_SWITCH_ANALYSIS.md` — analysis document, kept for reference

---

## v2.7.5 — Stable Build (2026-06)
*(previous version — C++ only, no custom kernel)*
- Initial shell + GUI implementation
- Linux-based init with main.cpp as `/init`
- Framebuffer GUI (XP Luna style)
- Purr++ interpreter
- KPM package manager
- Basic file system commands (cd, ls, mkdir, rm, touch, cat)
- Calculator, time, color/cursor customization
