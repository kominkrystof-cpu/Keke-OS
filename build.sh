#!/bin/bash
# Keke OS Build Script
# Runs all build steps with debug output
# Parts that need root will prompt for sudo when needed

set -e

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build"
TIMESTAMP=$(date '+%Y-%m-%d %H:%M:%S')

log() { echo -e "\033[36m[BUILD]\033[0m $1"; }
ok()  { echo -e "\033[32m[OK]\033[0m   $1"; }
warn(){ echo -e "\033[33m[WARN]\033[0m $1"; }
err() { echo -e "\033[31m[FAIL]\033[0m $1"; }
sep() { echo "----------------------------------------"; }

echo ""
echo "=========================================="
echo "  Keke OS Build System v2.7.5"
echo "  $TIMESTAMP"
echo "  Multi-Language Edition"
echo "=========================================="
echo ""

# ---- Step 0: Check prerequisites ----
log "Checking prerequisites..."
for cmd in g++ gcc make qemu-img parted grub-install; do
    if ! command -v $cmd &>/dev/null; then
        warn "$cmd not found (may not be needed)"
    fi
done
ok "Prerequisites check done"
sep

# ---- Step 1: Build userspace (C++ init + C programs) ----
log "[1/5] Building userspace programs..."
cd "$PROJECT_DIR/keke-src"
make clean 2>&1 | sed 's/^/  /'
make 2>&1 | sed 's/^/  /'
if [ -f "$BUILD_DIR/init" ]; then
    ok "C++ init built: $(ls -lh "$BUILD_DIR"/init | awk '{print $5}')"
fi
if [ -f "$BUILD_DIR/keke_info" ]; then
    ok "C program keke_info built: $(ls -lh "$BUILD_DIR"/keke_info | awk '{print $5}')"
fi
if [ -f "$BUILD_DIR/keketool" ]; then
    ok "C program keketool built: $(ls -lh "$BUILD_DIR"/keketool | awk '{print $5}')"
fi
sep

# ---- Step 2: Build kernel module ----
log "[2/5] Building Keke OS kernel module..."

# Kernel build system chokes on spaces in paths, so build in /tmp
MODULE_SRC_DIR="$PROJECT_DIR/keke-src/kekeos-mod"
MODULE_TMP_DIR="/tmp/kekeos-mod"

# Sync source to /tmp (preserve any existing build artifacts)
mkdir -p "$MODULE_TMP_DIR"
cp "$MODULE_SRC_DIR"/*.c "$MODULE_TMP_DIR/" 2>/dev/null || true
cp "$MODULE_SRC_DIR"/*.h "$MODULE_TMP_DIR/" 2>/dev/null || true
cp "$MODULE_SRC_DIR"/Makefile "$MODULE_TMP_DIR/" 2>/dev/null || true

cd "$MODULE_TMP_DIR"
make clean 2>&1 | sed 's/^/  /' || true
make 2>&1 | sed 's/^/  /'
if [ -f kekeos-mod.ko ]; then
    cp kekeos-mod.ko "$BUILD_DIR/"
    ok "kekeos-mod.ko: $(ls -lh "$BUILD_DIR"/kekeos-mod.ko | awk '{print $5}')"
else
    warn "Kernel module build failed"
fi
sep

# ---- Step 3: Build initramfs ----
log "[3/5] Building initramfs..."
cd "$PROJECT_DIR"
if [ "$EUID" -eq 0 ]; then
    bash build_initramfs.sh 2>&1 | sed 's/^/  /'
else
    warn "Not root — initramfs will skip device nodes + host modules"
    warn "Run 'sudo ./build_initramfs.sh' later if needed"
    bash build_initramfs.sh 2>&1 | sed 's/^/  /'
fi
if [ -f "$BUILD_DIR/keke-initramfs.cpio.gz" ]; then
    ok "Initramfs: $(ls -lh "$BUILD_DIR"/keke-initramfs.cpio.gz | awk '{print $5}')"
fi
sep

# ---- Step 4: Copy kernel ----
log "[4/5] Copying host kernel..."
cd "$PROJECT_DIR"
if [ -f "$BUILD_DIR/vmlinuz" ]; then
    ok "Kernel already present: $(ls -lh "$BUILD_DIR"/vmlinuz | awk '{print $5}')"
    if [ "$EUID" -eq 0 ]; then
        warn "Re-copying to get latest kernel..."
        bash copy_kernel.sh 2>&1 | sed 's/^/  /'
    fi
else
    if [ "$EUID" -eq 0 ]; then
        bash copy_kernel.sh 2>&1 | sed 's/^/  /'
    else
        warn "No kernel found and not root — run 'sudo ./copy_kernel.sh'"
        warn "Or use the vendored kernel source: linux-7.1.2/"
    fi
fi
sep

# ---- Step 5: Build disk image ----
log "[5/5] Building disk image..."
cd "$PROJECT_DIR"
if [ "$EUID" -eq 0 ]; then
    bash setup_disk.sh 2>&1 | sed 's/^/  /'
    ok "Disk image ready!"
else
    warn "Not root — cannot create disk image (needs loop device + mount)"
    warn "Run 'sudo ./setup_disk.sh' when ready"
    warn "Quick test (no disk): qemu-system-x86_64 -cdrom keke-os.iso -m 512 -serial stdio"
fi
sep

# ---- Summary ----
echo ""
echo "=========================================="
echo "  Build Summary"
echo "=========================================="
echo ""
echo "  Build files:"
ls -lh "$BUILD_DIR/" 2>/dev/null | awk '!/total/ && !/test_extract/ {print "    " $9 " (" $5 ")"}'
echo ""
echo "  Languages available at runtime:"
echo "    C++    /init (Keke Shell + GUI)"
echo "    C      /usr/bin/keke_info, /usr/bin/keketool"
echo "    JS     /mnt/bin/qjs (QuickJS) — if disk mounted"
echo "    Python /mnt/bin/python3 — if disk mounted"
echo "    Purr++ built into shell"
echo ""
echo "  Boot commands:"
echo "    Disk:   qemu-system-x86_64 -drive file=keke-disk.img,format=raw -m 512 -serial stdio"
echo "    ISO:    qemu-system-x86_64 -cdrom keke-os.iso -m 512 -serial stdio"
echo "    GUI:    qemu-system-x86_64 -drive file=keke-disk.img,format=raw -m 512 -vga std -serial stdio"
echo ""
echo "  If boot fails, run with: -enable-kvm -cpu host"
echo "=========================================="
echo ""
