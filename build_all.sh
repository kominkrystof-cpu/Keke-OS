#!/bin/bash

# Keke OS - Complete Build System
# Builds: kernel module, C programs, init binary, initramfs, disk image
# Language support: C++, C, Python, JavaScript (QuickJS), Purr++, Shell

set -e

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build"

echo "============================================"
echo "  Keke OS Build System v2.7.5"
echo "  Multi-Language Edition"
echo "============================================"
echo ""

# Step 1: Build kernel module
echo "[1/6] Building Keke OS kernel module (kekeos-mod.ko)..."
if [ -d /tmp/kekeos-mod ]; then
    make -C /tmp/kekeos-mod clean 2>/dev/null || true
    make -C /tmp/kekeos-mod 2>&1 | grep -v "^make"
    cp /tmp/kekeos-mod/kekeos-mod.ko "$BUILD_DIR/"
    echo "  -> build/kekeos-mod.ko"
else
    echo "  SKIP: /tmp/kekeos-mod not found. Run kekeos-mod/Makefile manually."
fi
echo ""

# Step 2: Build init binary + C programs
echo "[2/6] Building userspace programs (C++ + C)..."
make -C "$PROJECT_DIR/keke-src" 2>&1 | grep -E "(built:|error:|warning:)"
echo "  -> build/init (C++ init/PID 1)"
echo "  -> build/keke_info (C syscall demo)"
echo "  -> build/keketool (C syscall tool)"
echo ""

# Step 3: Build initramfs
echo "[3/6] Building initramfs (init + C programs + kernel modules)..."
bash "$PROJECT_DIR/build_initramfs.sh" 2>&1 | grep -E "(Initramfs|Size|Copying|Warning|Error)"
echo ""

# Step 4: Copy host kernel (for now - custom kernel build is optional)
echo "[4/6] Checking kernel..."
if [ -f "$BUILD_DIR/vmlinuz" ]; then
    echo "  Using existing kernel: build/vmlinuz"
else
    echo "  WARNING: No kernel found in build/. Run:"
    echo "    sudo ./copy_kernel.sh"
    echo "  Or build custom kernel from linux-7.1.2/"
fi
echo ""

# Step 5: Build disk image
echo "[5/6] Building disk image (interpreters + scripts)..."
if [ "$EUID" -eq 0 ]; then
    bash "$PROJECT_DIR/setup_disk.sh" 2>&1 | grep -E "(Keke OS|Error|->|Warning)"
else
    echo "  SKIP: needs root for loop device + mount"
    echo "  Run: sudo ./setup_disk.sh"
fi
echo ""

# Step 6: Show final state
echo "[6/6] Build summary:"
echo "============================================"
ls -lh "$BUILD_DIR/" 2>/dev/null | awk '{print "  " $9 " (" $5 ")"}'
echo ""
echo "Languages available at runtime:"
echo "  C++  -> /init (Keke Shell + GUI)"
echo "  C    -> /usr/bin/keke_info, /usr/bin/keketool"
echo "  JS   -> /mnt/bin/qjs (QuickJS)"
echo "  Py   -> /mnt/bin/python3"
echo "  Purr++ -> built into shell"
echo "  Sh   -> /bin/sh (via shell execution)"
echo ""
echo "To boot in QEMU (disk):"
echo "  qemu-system-x86_64 -drive file=keke-disk.img,format=raw -m 512 -serial stdio"
echo ""
echo "To boot in QEMU (ISO):"
echo "  qemu-system-x86_64 -cdrom keke-os.iso -m 512 -serial stdio"
echo "============================================"
