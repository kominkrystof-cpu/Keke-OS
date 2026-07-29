#!/bin/bash
# Deploy Keke OS kernel + initramfs to real-hardware partition (/dev/sda6)
# Usage: sudo ./deploy.sh [/dev/sda6]

set -e

DEVICE="${1:-/dev/sda6}"
MOUNT="/mnt"
PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build"

echo "[Keke OS] Deploy target: $DEVICE"

# ---------------------------------------------------------------------------
# Staleness check helper: warn if output is older than any input
# Usage: check_stale OUTPUT INPUT1 [INPUT2 ...]
# Returns 0 if output is newer than all inputs, 1 if stale or missing.
# ---------------------------------------------------------------------------
check_stale() {
    local output="$1"
    shift
    if [ ! -f "$output" ]; then
        echo "[Keke OS]   MISSING: $output — rebuild needed"
        return 1
    fi
    local out_mtime
    out_mtime=$(stat -c %Y "$output" 2>/dev/null || echo 0)
    local stale=0
    for src in "$@"; do
        if [ ! -f "$src" ]; then
            continue  # input doesn't exist, can't compare
        fi
        local src_mtime
        src_mtime=$(stat -c %Y "$src" 2>/dev/null || echo 0)
        if [ "$src_mtime" -gt "$out_mtime" ]; then
            echo "[Keke OS]   STALE: $output is older than $src"
            stale=1
        fi
    done
    return "$stale"
}

# ---------------------------------------------------------------------------
# Step 1: Build init binary (always — cheap, ~2s)
#  rm before make: git doesn't reliably update mtimes on checkout/pull,
#  so both check_stale and make's internal timestamp check can silently
#  skip rebuilds. Deleting the artifact forces a fresh build.
# ---------------------------------------------------------------------------
echo ""
echo "[Keke OS] === Step 1: Build init ==="
rm -f "$BUILD_DIR/init"
cd "$PROJECT_DIR/keke-src"
make
cd "$PROJECT_DIR"

# ---------------------------------------------------------------------------
# Step 2: Build initramfs (also copies matching host kernel to build/vmlinuz)
# ---------------------------------------------------------------------------
echo ""
echo "[Keke OS] === Step 2: Build initramfs ==="
echo "[Keke OS] Rebuilding initramfs (also refreshes kernel from host)..."
sudo bash "$PROJECT_DIR/build_initramfs.sh"

# ---------------------------------------------------------------------------
# Step 3: Deploy to partition
# ---------------------------------------------------------------------------
echo ""
echo "[Keke OS] === Step 3: Deploy to $DEVICE ==="

# Validate output files exist before mounting
if [ ! -f "$BUILD_DIR/vmlinuz" ]; then
    echo "[Keke OS] Error: $BUILD_DIR/vmlinuz not found"
    exit 1
fi
if [ ! -f "$BUILD_DIR/keke-initramfs.cpio.gz" ]; then
    echo "[Keke OS] Error: $BUILD_DIR/keke-initramfs.cpio.gz not found"
    exit 1
fi

echo "[Keke OS] All outputs present. Proceeding..."

# Mount the target partition
if mount | grep -q " $MOUNT "; then
    echo "[Keke OS] $MOUNT is already mounted"
else
    echo "[Keke OS] Mounting $DEVICE to $MOUNT..."
    mount "$DEVICE" "$MOUNT"
fi

# Ensure boot directory exists
mkdir -p "$MOUNT/boot"

# Copy kernel + initramfs
echo "[Keke OS] Copying vmlinuz -> $MOUNT/boot/"
cp "$BUILD_DIR/vmlinuz" "$MOUNT/boot/vmlinuz"

echo "[Keke OS] Copying keke-initramfs.cpio.gz -> $MOUNT/boot/"
cp "$BUILD_DIR/keke-initramfs.cpio.gz" "$MOUNT/boot/keke-initramfs.cpio.gz"

# Verify copy
echo "[Keke OS] Verifying..."
cmp "$BUILD_DIR/vmlinuz" "$MOUNT/boot/vmlinuz" \
    && echo "[Keke OS]   vmlinuz: OK" \
    || echo "[Keke OS]   vmlinuz: MISMATCH!"

cmp "$BUILD_DIR/keke-initramfs.cpio.gz" "$MOUNT/boot/keke-initramfs.cpio.gz" \
    && echo "[Keke OS]   initramfs: OK" \
    || echo "[Keke OS]   initramfs: MISMATCH!"

# Unmount
echo "[Keke OS] Unmounting $MOUNT..."
umount "$MOUNT"

echo ""
echo "[Keke OS] Deploy complete. You may now reboot."
echo "[Keke OS] Files deployed:"
ls -lh "$MOUNT/boot/vmlinuz" "$MOUNT/boot/keke-initramfs.cpio.gz" 2>/dev/null || true
