#!/bin/bash

# Build custom initramfs with Keke OS init + C programs + kernel modules

set -e

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build"
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
mkdir -p "$INITRAMFS_DIR/usr/bin"
mkdir -p "$INITRAMFS_DIR/usr/lib"
mkdir -p "$INITRAMFS_DIR/usr/share"

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

# Copy Keke OS kernel module
if [ -f "$BUILD_DIR/kekeos-mod.ko" ]; then
    echo "[Keke OS] Copying kekeos-mod.ko"
    cp "$BUILD_DIR/kekeos-mod.ko" "$INITRAMFS_DIR/lib/modules/kekeos-mod.ko"
fi

# Copy bochs DRM kernel module for QEMU VGA framebuffer
echo "[Keke OS] Copying bochs kernel module..."
BOCHS_MODULE_SRC="/lib/modules/$(uname -r)/kernel/drivers/gpu/drm/tiny/bochs.ko.zst"
if [ -f "$BOCHS_MODULE_SRC" ]; then
    zstd -d "$BOCHS_MODULE_SRC" -o "$INITRAMFS_DIR/lib/modules/bochs.ko" -f 2>/dev/null
    echo "[Keke OS] Copied bochs.ko (from .zst)"
else
    BOCHS_MODULE_SRC="/lib/modules/$(uname -r)/kernel/drivers/gpu/drm/tiny/bochs.ko"
    if [ -f "$BOCHS_MODULE_SRC" ]; then
        cp "$BOCHS_MODULE_SRC" "$INITRAMFS_DIR/lib/modules/bochs.ko"
        echo "[Keke OS] Copied bochs.ko"
    else
        echo "[Keke OS] Warning: bochs.ko not found, framebuffer may not work"
    fi
fi

# Copy psmouse module for PS/2 mouse support
echo "[Keke OS] Copying psmouse kernel module..."
PSMOUSE_MODULE_SRC="/lib/modules/$(uname -r)/kernel/drivers/input/mouse/psmouse.ko.zst"
if [ -f "$PSMOUSE_MODULE_SRC" ]; then
    zstd -d "$PSMOUSE_MODULE_SRC" -o "$INITRAMFS_DIR/lib/modules/psmouse.ko" -f 2>/dev/null
    echo "[Keke OS] Copied psmouse.ko (from .zst)"
else
    PSMOUSE_MODULE_SRC="/lib/modules/$(uname -r)/kernel/drivers/input/mouse/psmouse.ko"
    if [ -f "$PSMOUSE_MODULE_SRC" ]; then
        cp "$PSMOUSE_MODULE_SRC" "$INITRAMFS_DIR/lib/modules/psmouse.ko"
        echo "[Keke OS] Copied psmouse.ko"
    else
        echo "[Keke OS] Warning: psmouse.ko not found, mouse may not work"
    fi
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
