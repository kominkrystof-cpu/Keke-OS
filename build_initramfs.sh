#!/bin/bash

# Build custom initramfs with Keke OS init + C programs + kernel modules

set -e

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build"
INITRAMFS_DIR="initramfs_temp"
OUTPUT="build/keke-initramfs.cpio.gz"
KERNEL_VER="$(uname -r)"

echo "[Keke OS] Building custom initramfs..."
echo "[Keke OS] Kernel version: $KERNEL_VER"

# Check required tools
if ! command -v zstd &>/dev/null; then
    echo "[Keke OS] Error: zstd is required but not installed."
    echo "[Keke OS] Install it: sudo apt install zstd"
    exit 1
fi

# Clean up any existing temp directory
rm -rf "$INITRAMFS_DIR"

# Create initramfs directory structure
mkdir -p "$INITRAMFS_DIR"
mkdir -p "$INITRAMFS_DIR/bin"
mkdir -p "$INITRAMFS_DIR/sbin"
mkdir -p "$INITRAMFS_DIR/dev"
mkdir -p "$INITRAMFS_DIR/proc"
mkdir -p "$INITRAMFS_DIR/sys"
mkdir -p "$INITRAMFS_DIR/tmp"
mkdir -p "$INITRAMFS_DIR/mnt"
mkdir -p "$INITRAMFS_DIR/lib"
mkdir -p "$INITRAMFS_DIR/usr/bin"
mkdir -p "$INITRAMFS_DIR/usr/lib"
mkdir -p "$INITRAMFS_DIR/usr/share"

# List of required modules that must be present after build
REQUIRED_MODULES=()

# Copy a kernel module into the initramfs
# Usage: copy_module <kernel-module-path> <dest-name> [is-optional]
#   kernel-module-path: relative path under /lib/modules/$(uname -r)/kernel/
#                       e.g. "drivers/net/ethernet/intel/e1000e/e1000e"
#   dest-name:          filename in initramfs (e.g. "e1000e.ko")
#   is-optional:        if "optional", failure is a warning, not an error
copy_module() {
    local relpath="$1"
    local destname="$2"
    local optional="${3:-required}"
    local zst_src="/lib/modules/$KERNEL_VER/kernel/${relpath}.ko.zst"
    local ko_src="/lib/modules/$KERNEL_VER/kernel/${relpath}.ko"
    local dest="$INITRAMFS_DIR/lib/modules/$destname"

    if [ -f "$zst_src" ]; then
        zstd -d "$zst_src" -o "$dest" -f 2>/dev/null
        if [ $? -eq 0 ] && [ -f "$dest" ]; then
            echo "[Keke OS] Copied $destname (from .zst)"
            REQUIRED_MODULES+=("$destname")
            return 0
        else
            echo "[Keke OS] Warning: zstd decompression failed for $zst_src"
        fi
    fi

    if [ -f "$ko_src" ]; then
        cp "$ko_src" "$dest"
        echo "[Keke OS] Copied $destname"
        REQUIRED_MODULES+=("$destname")
        return 0
    fi

    if [ "$optional" = "optional" ]; then
        echo "[Keke OS] Warning: $destname not found (optional)"
    else
        echo "[Keke OS] Warning: $destname not found — module won't be available"
    fi
    return 1
}

# Copy our C++ init program
if [ -f "$BUILD_DIR/init" ]; then
    echo "[Keke OS] Copying init program..."
    cp "$BUILD_DIR/init" "$INITRAMFS_DIR/keke-init"
    chmod +x "$INITRAMFS_DIR/keke-init"
    cp "$BUILD_DIR/init" "$INITRAMFS_DIR/init"
    chmod +x "$INITRAMFS_DIR/init"
else
    echo "[Keke OS] Error: init program not found in build/"
    echo "[Keke OS] Run 'cd keke-src && make' first"
    exit 1
fi

# Copy C programs
for prog in keke_info keketool; do
    if [ -f "$BUILD_DIR/$prog" ]; then
        echo "[Keke OS] Copying C program: $prog"
        cp "$BUILD_DIR/$prog" "$INITRAMFS_DIR/usr/bin/$prog"
        chmod +x "$INITRAMFS_DIR/usr/bin/$prog"
    fi
done

# Create device nodes (skip if not root)
if [ "$EUID" -eq 0 ]; then
    mknod -m 666 "$INITRAMFS_DIR/dev/console" c 5 1
    mknod -m 666 "$INITRAMFS_DIR/dev/null" c 1 3
    mknod -m 666 "$INITRAMFS_DIR/dev/zero" c 1 5
    mknod -m 666 "$INITRAMFS_DIR/dev/tty" c 5 0
    mknod -m 666 "$INITRAMFS_DIR/dev/tty0" c 4 0
    mknod -m 620 "$INITRAMFS_DIR/dev/ttyS0" c 4 64
else
    echo "[Keke OS] Warning: Not root, skipping device node creation"
    echo "[Keke OS] Device nodes will be created by devtmpfs at boot"
fi

# Copy kernel modules
mkdir -p "$INITRAMFS_DIR/lib/modules"

# Copy Keke OS custom kernel module
if [ -f "$BUILD_DIR/kekeos-mod.ko" ]; then
    echo "[Keke OS] Copying kekeos-mod.ko"
    cp "$BUILD_DIR/kekeos-mod.ko" "$INITRAMFS_DIR/lib/modules/kekeos-mod.ko"
    REQUIRED_MODULES+=("kekeos-mod.ko")
fi

# QEMU framebuffer (optional — only needed in QEMU)
copy_module "drivers/gpu/drm/tiny/bochs" "bochs.ko" "optional"

# PS/2 mouse (optional — only needed for mouse support)
copy_module "drivers/input/mouse/psmouse" "psmouse.ko" "optional"

# e1000 — QEMU virtual NIC (optional)
copy_module "drivers/net/ethernet/intel/e1000/e1000" "e1000.ko" "optional"

# e1000e — real Intel NICs (I218-LM in X240, etc.)
copy_module "drivers/net/ethernet/intel/e1000e/e1000e" "e1000e.ko"

# Post-build validation: verify required modules landed
echo ""
MISSING=0
for mod in "${REQUIRED_MODULES[@]}"; do
    if [ ! -f "$INITRAMFS_DIR/lib/modules/$mod" ]; then
        echo "[Keke OS] VALIDATION FAILED: $mod missing from initramfs!"
        MISSING=1
    fi
done
if [ "$MISSING" -eq 1 ]; then
    echo "[Keke OS] Error: Required modules missing — aborting"
    echo "[Keke OS] Check that the kernel module paths exist for kernel $KERNEL_VER"
    rm -rf "$INITRAMFS_DIR"
    exit 1
fi

# Create the initramfs archive
echo "[Keke OS] Creating initramfs archive..."
cd "$INITRAMFS_DIR"
find . | cpio -o -H newc | gzip > "../$OUTPUT"
cd ..

# Cleanup
rm -rf "$INITRAMFS_DIR"

echo "[Keke OS] Initramfs created: $OUTPUT"
echo "[Keke OS] Size: $(ls -lh "$OUTPUT" | awk '{print $5}')"
