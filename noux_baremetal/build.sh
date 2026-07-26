#!/bin/bash
set -e

echo "[1/5] Assembling boot.asm..."
nasm -f elf32 boot.asm -o boot.o

echo "[2/5] Compiling noux.c (Bare-Metal with Gasty)..."
gcc -m32 -c noux.c -o noux.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra

echo "[3/5] Linking kernel binary..."
ld -m elf_i386 -T linker.ld boot.o noux.o -o noux_kernel.bin

echo "[4/5] Building Bootable ISO Image..."
mkdir -p iso/boot/grub
cp noux_kernel.bin iso/boot/noux_kernel.bin

cat << 'GRUB_EOF' > iso/boot/grub/grub.cfg
set timeout=3
set default=0

menuentry "Noux Bare-Metal OS (Gasty Inside)" {
    multiboot /boot/noux_kernel.bin
    boot
}
GRUB_EOF

grub-mkrescue -o noux.iso iso/ > /dev/null 2>&1

echo "[5/5] Launching Noux ISO in QEMU..."
qemu-system-i386 -cdrom noux.iso
