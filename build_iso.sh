#!/bin/bash

# Keke OS ISO Builder - creates a bootable ISO for real hardware (USB, CD, etc.)
# Uses grub-mkrescue to produce a hybrid BIOS+UEFI bootable ISO

set -e

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build"
ISO_DIR="iso_temp"
OUTPUT_ISO="$BUILD_DIR/keke-os.iso"

echo "[Keke OS] Building bootable ISO..."

# Clean up any existing temp directory
rm -rf "$ISO_DIR"

# Create ISO directory structure
mkdir -p "$ISO_DIR/boot/grub"

# Copy kernel and initramfs
if [ -f "$BUILD_DIR/vmlinuz" ]; then
    cp "$BUILD_DIR/vmlinuz" "$ISO_DIR/boot/vmlinuz"
    echo "[Keke OS] Copied vmlinuz"
else
    echo "[Keke OS] Error: vmlinuz not found in build/"
    exit 1
fi

if [ -f "$BUILD_DIR/keke-initramfs.cpio.gz" ]; then
    cp "$BUILD_DIR/keke-initramfs.cpio.gz" "$ISO_DIR/boot/keke-initramfs.cpio.gz"
    echo "[Keke OS] Copied keke-initramfs.cpio.gz"
else
    echo "[Keke OS] Error: keke-initramfs.cpio.gz not found in build/"
    exit 1
fi

# Create GRUB config for ISO boot
cat > "$ISO_DIR/boot/grub/grub.cfg" << 'GRUB_EOF'
set timeout=5
set default=0

menuentry "Keke OS v2.7.5" {
    set gfxpayload=keep
    linux /boot/vmlinuz loglevel=3
    initrd /boot/keke-initramfs.cpio.gz
}

menuentry "Keke OS - Safe Mode (No Framebuffer)" {
    linux /boot/vmlinuz nomodeset text loglevel=3
    initrd /boot/keke-initramfs.cpio.gz
}

menuentry "Keke OS - Debug Mode" {
    linux /boot/vmlinuz earlyprintk debug ignore_loglevel loglevel=1
    initrd /boot/keke-initramfs.cpio.gz
}
GRUB_EOF

echo "[Keke OS] Created GRUB config"

# Build the ISO using grub-mkrescue
echo "[Keke OS] Running grub-mkrescue (BIOS+UEFI)..."
grub-mkrescue -o "$OUTPUT_ISO" "$ISO_DIR" 2>&1

# Cleanup
rm -rf "$ISO_DIR"

# Verify
if [ -f "$OUTPUT_ISO" ]; then
    echo ""
    echo "[Keke OS] ISO created: $OUTPUT_ISO"
    echo "[Keke OS] Size: $(ls -lh "$OUTPUT_ISO" | awk '{print $5}')"
    echo ""
    echo "[Keke OS] To write to USB:"
    echo "  sudo dd if=$OUTPUT_ISO of=/dev/sdX bs=4M status=progress"
    echo "  (replace sdX with your USB device, e.g. sdb, sdc)"
    echo ""
    echo "[Keke OS] Boot on real hardware (X240):"
    echo "  1. Write ISO to USB stick"
    echo "  2. Insert into X240, boot from USB (F12 for boot menu)"
    echo "  3. Network won't work without the correct NIC driver"
    echo ""
else
    echo "[Keke OS] Error: ISO creation failed"
    exit 1
fi
