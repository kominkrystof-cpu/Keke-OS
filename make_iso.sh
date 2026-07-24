#!/bin/bash

# Alternative method using grub-mkrescue to create a bootable ISO
# This is often simpler than manual disk image setup

set -e

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
ISO_OUTPUT="keke-os.iso"
TEMP_DIR="iso_temp"

echo "[Keke OS] Creating bootable ISO with grub-mkrescue..."

# Create temporary directory structure
mkdir -p "$TEMP_DIR/boot/grub"
mkdir -p "$TEMP_DIR/boot"

# Copy kernel and initramfs
if [ -f "$PROJECT_DIR/build/vmlinuz" ]; then
    cp "$PROJECT_DIR/build/vmlinuz" "$TEMP_DIR/boot/"
else
    echo "[Keke OS] Error: vmlinuz not found in build/"
    echo "[Keke OS] Run ./copy_kernel.sh first"
    exit 1
fi

if [ -f "$PROJECT_DIR/build/keke-initramfs.cpio.gz" ]; then
    cp "$PROJECT_DIR/build/keke-initramfs.cpio.gz" "$TEMP_DIR/boot/"
else
    echo "[Keke OS] Warning: keke-initramfs.cpio.gz not found in build/"
fi

# Copy grub.cfg
cp "$PROJECT_DIR/grub.cfg" "$TEMP_DIR/boot/grub/"

# Create ISO
echo "[Keke OS] Generating ISO image..."
grub-mkrescue -o "$ISO_OUTPUT" "$TEMP_DIR" 2>/dev/null || {
    echo "[Keke OS] Trying with xorriso..."
    grub-mkrescue -o "$ISO_OUTPUT" "$TEMP_DIR" --xorriso xorriso
}

# Cleanup
rm -rf "$TEMP_DIR"

echo "[Keke OS] ISO created: $ISO_OUTPUT"
echo "[Keke OS] Boot with: qemu-system-x86_64 -cdrom $ISO_OUTPUT -m 512 -serial stdio"
