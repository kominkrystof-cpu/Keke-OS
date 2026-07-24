#!/bin/bash

# Simplified disk setup - creates a disk image with filesystem directly (no partitioning)
# This is sufficient for QEMU booting

set -e

DISK_IMG="keke-disk.img"
DISK_SIZE="256M"
MOUNT_POINT="mnt_temp"
PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "[Keke OS] Setting up disk image (simplified method)..."

# Create disk image with ext4 filesystem directly
if [ -f "$DISK_IMG" ]; then
    echo "[Keke OS] Removing existing disk image..."
    rm -f "$DISK_IMG"
fi

echo "[Keke OS] Creating disk image ($DISK_SIZE)..."
dd if=/dev/zero of="$DISK_IMG" bs=1M count=256 status=none

echo "[Keke OS] Creating ext4 filesystem..."
mkfs.ext4 -F "$DISK_IMG"

# Setup loop device
echo "[Keke OS] Setting up loop device..."
LOOP_DEV=$(losetup --find --show "$DISK_IMG")

if [ -z "$LOOP_DEV" ]; then
    echo "[Keke OS] Error: Could not setup loop device"
    exit 1
fi

# Cleanup function
cleanup() {
    echo "[Keke OS] Cleaning up..."
    if [ -d "$MOUNT_POINT" ]; then
        umount "$MOUNT_POINT" 2>/dev/null || true
        rmdir "$MOUNT_POINT" 2>/dev/null || true
    fi
    if [ -n "$LOOP_DEV" ]; then
        losetup -d "$LOOP_DEV" 2>/dev/null || true
    fi
}

trap cleanup EXIT

# Create mount point
mkdir -p "$MOUNT_POINT"

# Mount filesystem
echo "[Keke OS] Mounting filesystem..."
mount "$LOOP_DEV" "$MOUNT_POINT"

# Create directory structure
echo "[Keke OS] Creating directory structure..."
mkdir -p "$MOUNT_POINT/boot/grub"
mkdir -p "$MOUNT_POINT/mnt"

# Copy kernel and initramfs
echo "[Keke OS] Copying kernel and initramfs..."
if [ -f "$PROJECT_DIR/build/vmlinuz" ]; then
    cp "$PROJECT_DIR/build/vmlinuz" "$MOUNT_POINT/boot/"
else
    echo "[Keke OS] Warning: vmlinuz not found in build/"
fi

if [ -f "$PROJECT_DIR/build/keke-initramfs.cpio.gz" ]; then
    cp "$PROJECT_DIR/build/keke-initramfs.cpio.gz" "$MOUNT_POINT/boot/"
else
    echo "[Keke OS] Warning: keke-initramfs.cpio.gz not found in build/"
fi

# Copy init program
if [ -f "$PROJECT_DIR/build/init" ]; then
    cp "$PROJECT_DIR/build/init" "$MOUNT_POINT/init"
else
    echo "[Keke OS] Warning: init not found in build/"
fi

# Copy disk content
if [ -d "$PROJECT_DIR/disk_content" ]; then
    cp -r "$PROJECT_DIR/disk_content"/* "$MOUNT_POINT/mnt/" 2>/dev/null || true
fi

# Copy grub.cfg
echo "[Keke OS] Installing GRUB configuration..."
cp "$PROJECT_DIR/grub.cfg" "$MOUNT_POINT/boot/grub/"

# Note: GRUB installation requires partition table
# For QEMU without partition, we'll use a different approach
echo "[Keke OS] Disk image ready for QEMU (no GRUB installed)"
echo "[Keke OS] Boot with: qemu-system-x86_64 -kernel build/vmlinuz -initrd build/keke-initramfs.cpio.gz -drive file=$DISK_IMG,format=raw -m 512 -serial stdio -append \"root=/dev/sda ro\""
