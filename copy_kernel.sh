#!/bin/bash

# Script to copy Ubuntu kernel from Zorin OS
# This copies the kernel and initramfs from the host system to the build directory

set -e

BUILD_DIR="$(cd "$(dirname "$0")/build" && pwd)"
HOST_KERNEL="/boot/vmlinuz-$(uname -r)"
HOST_INITRD="/boot/initrd.img-$(uname -r)"

echo "[Keke OS] Copying kernel from host system..."

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "[Keke OS] This script requires root privileges to copy from /boot"
    echo "[Keke OS] Please run with: sudo $0"
    exit 1
fi

# Copy kernel
if [ -f "$HOST_KERNEL" ]; then
    echo "[Keke OS] Copying kernel: $HOST_KERNEL"
    cp "$HOST_KERNEL" "$BUILD_DIR/vmlinuz"
else
    echo "[Keke OS] Error: Kernel not found at $HOST_KERNEL"
    echo "[Keke OS] Available kernels in /boot:"
    ls -1 /boot/vmlinuz-* 2>/dev/null || echo "  None found"
    exit 1
fi

# Copy initramfs
if [ -f "$HOST_INITRD" ]; then
    echo "[Keke OS] Copying initramfs: $HOST_INITRD"
    cp "$HOST_INITRD" "$BUILD_DIR/keke-initramfs.cpio.gz"
else
    echo "[Keke OS] Warning: Initramfs not found at $HOST_INITRD"
    echo "[Keke OS] Available initramfs in /boot:"
    ls -1 /boot/initrd.img-* 2>/dev/null || echo "  None found"
fi

echo "[Keke OS] Kernel copy complete!"
