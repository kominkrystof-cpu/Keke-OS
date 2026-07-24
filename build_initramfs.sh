#!/bin/bash

# Build custom initramfs with Keke OS C++ init program

set -e

BUILD_DIR="$(cd "$(dirname "$0")/build" && pwd)"
INITRAMFS_DIR="initramfs_temp"
OUTPUT="build/keke-initramfs.cpio.gz"

echo "[Keke OS] Building custom initramfs..."

# Clean up any existing temp directory
rm -rf "$INITRAMFS_DIR"

# Create initramfs directory structure
mkdir -p "$INITRAMFS_DIR"
mkdir -p "$INITRAMFS_DIR/bin"
mkdir -p "$INITRAMFS_DIR/sbin"
mkdir -p "$INITRAMFS_DIR/dev"
mkdir -p "$INITRAMFS_DIR/proc"
mkdir -p "$INITRAMFS_DIR/sys"
mkdir -p "$INITRAMFS_DIR/mnt"
mkdir -p "$INITRAMFS_DIR/lib"

# Copy our C++ init program to /keke-init (not /init)
if [ -f "$BUILD_DIR/init" ]; then
    echo "[Keke OS] Copying init program..."
    cp "$BUILD_DIR/init" "$INITRAMFS_DIR/keke-init"
    chmod +x "$INITRAMFS_DIR/keke-init"
    # Also copy to /init as fallback
    cp "$BUILD_DIR/init" "$INITRAMFS_DIR/init"
    chmod +x "$INITRAMFS_DIR/init"
else
    echo "[Keke OS] Error: init program not found in build/"
    echo "[Keke OS] Run 'cd keke-src && make' first"
    exit 1
fi

# Create device nodes
mknod -m 666 "$INITRAMFS_DIR/dev/console" c 5 1
mknod -m 666 "$INITRAMFS_DIR/dev/null" c 1 3
mknod -m 666 "$INITRAMFS_DIR/dev/zero" c 1 5
mknod -m 666 "$INITRAMFS_DIR/dev/tty" c 5 0
mknod -m 666 "$INITRAMFS_DIR/dev/tty0" c 4 0
mknod -m 620 "$INITRAMFS_DIR/dev/ttyS0" c 4 64
# Don't create fb0 manually - let the kernel create it when framebuffer is loaded

# Copy kernel modules
mkdir -p "$INITRAMFS_DIR/lib/modules"

# Copy bochs DRM kernel module for QEMU VGA framebuffer support
echo "[Keke OS] Copying bochs kernel module..."
BOCHS_MODULE_SRC="/lib/modules/$(uname -r)/kernel/drivers/gpu/drm/tiny/bochs.ko.zst"
if [ -f "$BOCHS_MODULE_SRC" ]; then
    zstd -d "$BOCHS_MODULE_SRC" -o "$INITRAMFS_DIR/lib/modules/bochs.ko" -f 2>/dev/null
    echo "[Keke OS] Copied bochs.ko"
else
    BOCHS_MODULE_SRC="/lib/modules/$(uname -r)/kernel/drivers/gpu/drm/tiny/bochs.ko"
    if [ -f "$BOCHS_MODULE_SRC" ]; then
        cp "$BOCHS_MODULE_SRC" "$INITRAMFS_DIR/lib/modules/bochs.ko"
        echo "[Keke OS] Copied bochs.ko"
    else
        echo "[Keke OS] Warning: bochs.ko not found, framebuffer may not work"
    fi
fi

# Copy psmouse module for PS/2 mouse support (CONFIG_MOUSE_PS2=m)
echo "[Keke OS] Copying psmouse kernel module..."
PSMOUSE_MODULE_SRC="/lib/modules/$(uname -r)/kernel/drivers/input/mouse/psmouse.ko.zst"
if [ -f "$PSMOUSE_MODULE_SRC" ]; then
    zstd -d "$PSMOUSE_MODULE_SRC" -o "$INITRAMFS_DIR/lib/modules/psmouse.ko" -f 2>/dev/null
    echo "[Keke OS] Copied psmouse.ko"
else
    PSMOUSE_MODULE_SRC="/lib/modules/$(uname -r)/kernel/drivers/input/mouse/psmouse.ko"
    if [ -f "$PSMOUSE_MODULE_SRC" ]; then
        cp "$PSMOUSE_MODULE_SRC" "$INITRAMFS_DIR/lib/modules/psmouse.ko"
        echo "[Keke OS] Copied psmouse.ko"
    else
        echo "[Keke OS] Warning: psmouse.ko not found, mouse may not work"
    fi
fi

# The C++ binary is compiled with -static, so no shared libs to copy.
# Don't create shell script init - use C++ program directly

# Create the initramfs archive
echo "[Keke OS] Creating initramfs archive..."
cd "$INITRAMFS_DIR"
find . | cpio -o -H newc | gzip > "../$OUTPUT"
cd ..

# Cleanup
rm -rf "$INITRAMFS_DIR"

echo "[Keke OS] Initramfs created: $OUTPUT"
