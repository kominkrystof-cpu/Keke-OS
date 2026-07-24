# Keke OS - GRUB Installation Guide

## Overview
This guide explains how to set up GRUB for Keke OS to boot in QEMU.

## Prerequisites
- Linux host system (Zorin OS or Ubuntu-based)
- QEMU installed
- GRUB tools installed: `grub-pc-bin`, `grub-common`, `xorriso`
- Root/sudo access for copying kernel

## Quick Start

### Method 1: Using grub-mkrescue (Recommended for QEMU)

1. **Copy the kernel from your host system:**
   ```bash
   sudo ./copy_kernel.sh
   ```

2. **Build your C++ init program:**
   ```bash
   cd keke-src
   make
   cd ..
   ```

3. **Create bootable ISO:**
   ```bash
   ./make_iso.sh
   ```

4. **Boot in QEMU:**
   ```bash
   qemu-system-x86_64 -cdrom keke-os.iso -m 512 -serial stdio -vga std
   ```

### Method 2: Using Disk Image with GRUB Installation

1. **Copy the kernel:**
   ```bash
   sudo ./copy_kernel.sh
   ```

2. **Build your init program:**
   ```bash
   cd keke-src
   make
   cd ..
   ```

3. **Setup disk image and install GRUB:**
   ```bash
   sudo ./setup_disk.sh
   ```

4. **Boot in QEMU:**
   ```bash
   qemu-system-x86_64 -drive file=keke-disk.img,format=raw -m 512 -serial stdio -vga std
   ```

## GRUB Configuration

The `grub.cfg` file includes three boot options:

1. **Keke OS v2.7.5 - Normal Mode**: Standard boot with framebuffer support
2. **Keke OS - Safe Mode**: No framebuffer, text mode only
3. **Keke OS - Debug Mode**: Verbose serial output for debugging

## Directory Structure

```
Keke-OS/
├── grub.cfg              # GRUB configuration
├── keke-disk.img         # Disk image (created by setup_disk.sh)
├── keke-os.iso           # ISO image (created by make_iso.sh)
├── keke-src/
│   ├── main.cpp          # C++ init program
│   └── Makefile
├── build/
│   ├── init              # Compiled C++ program
│   ├── vmlinuz           # Linux kernel (copied from host)
│   └── keke-initramfs.cpio.gz  # Initramfs (copied from host)
├── disk_content/         # Files to include in /mnt
├── copy_kernel.sh        # Script to copy kernel from host
├── make_iso.sh           # Script to create ISO with grub-mkrescue
└── setup_disk.sh         # Script to setup disk image with GRUB
```

## Troubleshooting

### Kernel not found
- Ensure you've run `sudo ./copy_kernel.sh`
- Check that `/boot/vmlinuz-$(uname -r)` exists on your host

### GRUB installation fails
- Ensure you have `grub-pc-bin` and `grub-common` installed
- Try Method 1 (ISO) instead, which is simpler

### QEMU won't boot
- Add `-vga std` or `-vga virtio` for framebuffer support
- Use `-serial stdio` to see boot messages
- Increase memory with `-m 1024` if needed

### Framebuffer not working
- Try Safe Mode entry in GRUB
- Check that your C++ program properly handles `/dev/fb0`
- Ensure QEMU is started with VGA support

## Building the Init Program

The C++ init program is built with:
```bash
cd keke-src
make
```

This creates a static binary at `../build/init` that will be the first userspace process.

## Next Steps

1. Build your init program with `cd keke-src && make`
2. Copy the kernel with `sudo ./copy_kernel.sh`
3. Create bootable media with `./make_iso.sh` or `./setup_disk.sh`
4. Test in QEMU

## Customization

Edit `grub.cfg` to:
- Change boot timeout
- Add kernel parameters
- Add custom boot entries
- Change default boot option
