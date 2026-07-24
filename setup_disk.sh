#!/bin/bash

# Keke OS Disk Setup Script
# This script partitions the disk image, creates filesystems, and installs GRUB

set -e

DISK_IMG="keke-disk.img"
DISK_SIZE="64M"
MOUNT_POINT="mnt_temp"
PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "[Keke OS] Setting up disk image..."

# Check if disk image exists, if not create it
if [ ! -f "$DISK_IMG" ]; then
    echo "[Keke OS] Creating new disk image ($DISK_SIZE)..."
    qemu-img create -f raw "$DISK_IMG" "$DISK_SIZE"
fi

# Setup loop device
echo "[Keke OS] Setting up loop device..."
LOOP_DEV=$(losetup --find --show "$DISK_IMG" 2>/dev/null || true)

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

# Partition the disk using parted
echo "[Keke OS] Partitioning disk..."
parted -s "$LOOP_DEV" mklabel msdos
parted -s "$LOOP_DEV" mkpart primary ext4 1MiB 100%
parted -s "$LOOP_DEV" set 1 boot on

# Wait for partition to appear
sleep 2
partprobe "$LOOP_DEV" 2>/dev/null || true

# Format partition as ext4
echo "[Keke OS] Creating ext4 filesystem..."
PARTITION="${LOOP_DEV}p1"
if [ ! -b "$PARTITION" ]; then
    PARTITION="${LOOP_DEV}1"
fi

mkfs.ext4 -F "$PARTITION"

# Create mount point
mkdir -p "$MOUNT_POINT"

# Mount partition
echo "[Keke OS] Mounting partition..."
mount "$PARTITION" "$MOUNT_POINT"

# Create directory structure
echo "[Keke OS] Creating directory structure..."
mkdir -p "$MOUNT_POINT/boot"
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
    cp "$PROJECT_DIR/build/init" "$MOUNT_POINT/"
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

# Install GRUB
echo "[Keke OS] Installing GRUB to disk image..."
grub-install --target=i386-pc --boot-directory="$MOUNT_POINT/boot" "$LOOP_DEV" --modules="ext2 part_msdos" || {
    echo "[Keke OS] Trying alternative GRUB installation..."
    grub-install --target=i386-pc --boot-directory="$MOUNT_POINT/boot" "$LOOP_DEV"
}

echo "[Keke OS] Disk setup complete!"
echo "[Keke OS] You can now boot with: qemu-system-x86_64 -drive file=$DISK_IMG,format=raw -m 512 -serial stdio"
