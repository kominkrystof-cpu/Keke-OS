# GPL Compliance — Keke OS Custom Linux Kernel

## Background

Keke OS uses the **Linux kernel** under the **GNU General Public License v2 (GPL-2.0)**.
The vendored kernel source is at `linux-7.1.2/` (based on Linux kernel, licensed GPL-2.0).

We have **modified** the Linux kernel source to add custom system calls.
Under the GPL, any distribution of a binary built from modified GPL source
requires that the corresponding source code be made available.

## What We Changed

All modifications to the Linux kernel source are in **`linux-7.1.2/`**.
Here is an exhaustive list of every file we changed or added:

### New Files

| File | Description |
|------|-------------|
| `linux-7.1.2/kernel/keke.c` | Custom syscall implementation (`sys_keke_cmd`) — 6 sub-commands |
| `linux-7.1.2/include/uapi/linux/keke.h` | Userspace API header for the custom syscall |

### Modified Files

| File | Change |
|------|--------|
| `linux-7.1.2/arch/x86/entry/syscalls/syscall_64.tbl` | Added syscall #472: `keke_cmd` |
| `linux-7.1.2/kernel/Makefile` | Added `keke.o` to `obj-y` |
| `linux-7.1.2/include/linux/syscalls.h` | Added `asmlinkage long sys_keke_cmd(...)` declaration |

### Complete Diff

The entire custom syscall implementation is in `linux-7.1.2/kernel/keke.c` (approximately
60 lines of functional code). The UAPI header is 30 lines of struct definitions.
Total: ~90 lines added across 5 files.

### What We Did NOT Change

- No drivers were modified
- No architectures other than x86_64 were affected
- No existing kernel functionality was altered
- The GPL license headers in all modified files were preserved
- The kernel `COPYING` file (GPL-2.0) remains untouched

---

## Independent Kernel Module

The file `keke-src/kekeos-mod/kekeos_dev.c` is a **loadable kernel module**
compiled separately from the kernel. It provides the same interface as the
custom syscall via `/dev/kekeos`. This module is:

- Built against the **host kernel** (Zorin OS / Ubuntu), not our custom kernel
- Licensed GPL-2.0 (as required by Linux kernel module API)
- A separate derived work under GPL terms

---

## Source Availability

If you distribute Keke OS (as ISO, disk image, or binary kernel), you must:

1. Include this `LICENSE_COMPLIANCE.md` file
2. Provide access to the complete modified kernel source at `linux-7.1.2/`
3. Provide the kernel module source at `keke-src/kekeos-mod/`
4. Include the GPL-2.0 license text (already at `linux-7.1.2/COPYING`)

The source can be provided by:
- Including the full `linux-7.1.2/` directory (already done — it's in the repo)
- Or linking to a public repository (e.g., GitHub)

### Quick Verification

To see exactly what was changed vs upstream Linux:

```bash
# Already in this repo — compare against your upstream Linux source:
diff -rq /path/to/clean/linux-7.1.2 linux-7.1.2/ | grep -v ".o$\|.cmd$\|.d$\|.cache$\|tags$\|cscope$\|Module.symvers"
```

---

## Contact

For questions about GPL compliance in Keke OS:
- Project: Keke OS (keke@keke-os.com)
- Linux kernel: https://www.kernel.org
- GPL-2.0: https://www.gnu.org/licenses/old-licenses/gpl-2.0.html
