#!/bin/bash

# Keke OS Disk Setup Script
# Partitions disk image, creates fs, installs GRUB, populates with multi-lang tools

set -e

DISK_IMG="keke-disk.img"
DISK_SIZE="256M"
MOUNT_POINT="mnt_temp"
PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build"

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

# Partition the disk
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
mkdir -p "$MOUNT_POINT/bin"
mkdir -p "$MOUNT_POINT/scripts"
mkdir -p "$MOUNT_POINT/lib"

# Staleness check helper
check_stale() {
    local output="$1"
    shift
    if [ ! -f "$output" ]; then echo "  MISSING"; return 1; fi
    local out_mtime=$(stat -c %Y "$output" 2>/dev/null || echo 0)
    for src in "$@"; do
        [ ! -f "$src" ] && continue
        local src_mtime=$(stat -c %Y "$src" 2>/dev/null || echo 0)
        if [ "$src_mtime" -gt "$out_mtime" ]; then echo "  STALE vs $src"; return 1; fi
    done
    return 0
}

# Copy kernel and initramfs
echo "[Keke OS] Copying kernel and initramfs..."
STALE=0
if [ -f "$BUILD_DIR/vmlinuz" ]; then
    if check_stale "$BUILD_DIR/vmlinuz" "$PROJECT_DIR/keke-src/main.cpp" >/dev/null; then
        cp "$BUILD_DIR/vmlinuz" "$MOUNT_POINT/boot/"
    else
        echo "[Keke OS] Warning: vmlinuz may be stale — deploy anyway?"
        cp "$BUILD_DIR/vmlinuz" "$MOUNT_POINT/boot/"
        STALE=1
    fi
else
    echo "[Keke OS] Warning: vmlinuz not found in build/"
fi

if [ -f "$BUILD_DIR/keke-initramfs.cpio.gz" ]; then
    if check_stale "$BUILD_DIR/keke-initramfs.cpio.gz" "$BUILD_DIR/init" "$PROJECT_DIR/build_initramfs.sh" "$PROJECT_DIR/keke-src/main.cpp" >/dev/null; then
        cp "$BUILD_DIR/keke-initramfs.cpio.gz" "$MOUNT_POINT/boot/"
    else
        echo "[Keke OS] Warning: keke-initramfs.cpio.gz may be stale — deploy anyway?"
        cp "$BUILD_DIR/keke-initramfs.cpio.gz" "$MOUNT_POINT/boot/"
        STALE=1
    fi
else
    echo "[Keke OS] Warning: keke-initramfs.cpio.gz not found in build/"
fi

if [ "$STALE" -eq 1 ]; then
    echo "[Keke OS] Warning: Some artifacts were stale — consider rebuilding before testing"
fi

# Copy C programs to /mnt/bin
echo "[Keke OS] Copying C programs..."
for prog in keke_info keketool; do
    if [ -f "$BUILD_DIR/$prog" ]; then
        cp "$BUILD_DIR/$prog" "$MOUNT_POINT/bin/$prog"
        chmod +x "$MOUNT_POINT/bin/$prog"
        echo "[Keke OS]  -> /mnt/bin/$prog"
    fi
done

# Copy QuickJS interpreter to /mnt/bin
if [ -f "$BUILD_DIR/qjs" ]; then
    cp "$BUILD_DIR/qjs" "$MOUNT_POINT/bin/qjs"
    chmod +x "$MOUNT_POINT/bin/qjs"
    echo "[Keke OS]  -> /mnt/bin/qjs (QuickJS JS interpreter)"
fi

# Copy Python interpreter + shared libs to /mnt
if [ -f /usr/bin/python3 ]; then
    echo "[Keke OS] Copying Python interpreter..."
    cp /usr/bin/python3 "$MOUNT_POINT/bin/python3"
    chmod +x "$MOUNT_POINT/bin/python3"

    # Copy required shared libraries for Python
    for lib in /lib/x86_64-linux-gnu/libm.so.6 \
               /lib/x86_64-linux-gnu/libz.so.1 \
               /lib/x86_64-linux-gnu/libexpat.so.1 \
               /lib/x86_64-linux-gnu/libc.so.6 \
               /lib64/ld-linux-x86-64.so.2; do
        if [ -f "$lib" ]; then
            mkdir -p "$MOUNT_POINT$(dirname $lib)"
            cp "$lib" "$MOUNT_POINT$lib"
        fi
    done
    echo "[Keke OS]  -> /mnt/bin/python3 + shared libs"
fi

# Copy disk content (from disk_content/ directory)
if [ -d "$PROJECT_DIR/disk_content" ]; then
    echo "[Keke OS] Copying disk_content/..."
    cp -r "$PROJECT_DIR/disk_content/"* "$MOUNT_POINT/" 2>/dev/null || true
fi

# Copy scripts
if [ -d "$PROJECT_DIR/disk_content/scripts" ]; then
    echo "[Keke OS] Copying scripts..."
    cp -r "$PROJECT_DIR/disk_content/scripts/"* "$MOUNT_POINT/scripts/" 2>/dev/null || true
    chmod +x "$MOUNT_POINT/scripts/"*.sh 2>/dev/null || true
    chmod +x "$MOUNT_POINT/scripts/"*.py 2>/dev/null || true
fi

# Copy babicka.txt
if [ -f "$PROJECT_DIR/disk_content/babicka.txt" ]; then
    cp "$PROJECT_DIR/disk_content/babicka.txt" "$MOUNT_POINT/" 2>/dev/null || true
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

# Show disk contents
echo ""
echo "[Keke OS] Disk contents:"
find "$MOUNT_POINT" -type f | head -30

echo ""
echo "[Keke OS] Disk setup complete!"
echo "[Keke OS] Boot with: qemu-system-x86_64 -drive file=$DISK_IMG,format=raw -m 512 -serial stdio"
