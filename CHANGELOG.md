# Changelog

## v2.7.6 — Git Hygiene + GUI Architecture Review (2026-07-26)

### Repository
- **Added `TXT.txt` to `.gitignore`** — prevents template file from being tracked

### GUI Architecture (keke-src/gui.hpp)
- **Analyzed current C++ framebuffer GUI** — ~1,100 lines, Windows XP Luna style
- **Decision: Keep custom C++ implementation** — unique OS personality, no external deps, existing double-buffering + dirty-region optimization is solid
- **Future improvements identified**: font atlas for text rendering, layout system, retained-mode scene graph

### Documentation
- Updated CHANGELOG.md with this entry

---

## v2.7.5 — Multi-Language + Custom Kernel Edition (2026-07-24)

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
