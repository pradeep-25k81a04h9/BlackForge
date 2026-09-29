#!/bin/bash

set -e

echo "========================================"
echo "        BLACKFORGE BUILD SYSTEM"
echo "========================================"

echo "[1/16] Building terminal..."
gcc -m32 -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables \
    -c kernel/terminal.c -o kernel/terminal.o

echo "[2/16] Building keyboard..."
gcc -m32 -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables \
    -c kernel/keyboard.c -o kernel/keyboard.o

echo "[3/16] Building shell..."
gcc -m32 -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables \
    -c kernel/shell.c -o kernel/shell.o

echo "[4/16] Building kernel..."
gcc -m32 -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables \
    -c kernel/kernel.c -o kernel/kernel.o

echo "[5/16] Building memory manager..."
gcc -m32 -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables \
    -c kernel/memory.c -o kernel/memory.o

echo "[6/16] Building frame allocator..."
gcc -m32 -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables \
    -c kernel/frame.c -o kernel/frame.o

echo "[7/16] Building paging..."
gcc -m32 -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables \
    -c kernel/paging.c -o kernel/paging.o

echo "[8/16] Building kernel heap..."
gcc -m32 -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables \
    -c kernel/heap.c -o kernel/heap.o

echo "[9/16] Building Hindsight event system..."
gcc -m32 -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables \
    -c hindsight/event.c -o hindsight/event.o

echo "[10/16] Building Hindsight history..."
gcc -m32 -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables \
    -c hindsight/history.c -o hindsight/history.o

echo "[11/16] Building Hindsight incident..."
gcc -m32 -ffreestanding -fno-pie -fno-stack-protector \
    -fno-asynchronous-unwind-tables \
    -c hindsight/incident.c -o hindsight/incident.o

echo "[12/16] Building timer..."
gcc -m32 -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables \
    -c kernel/timer.c -o kernel/timer.o

echo "[13/16] Building scheduler..."
gcc -m32 -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables \
    -c kernel/scheduler.c -o kernel/scheduler.o

echo "[14/16] Building context switcher..."
nasm -f elf32 kernel/context.asm -o kernel/context.o

echo "[15/16] Building task manager..."
gcc -m32 -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables \
    -fno-omit-frame-pointer \
    -c kernel/task.c -o kernel/task.o

echo "[16/16] Building interrupts..."
gcc -m32 -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables \
    -c kernel/interrupts.c -o kernel/interrupts_c.o

nasm -f elf32 kernel/interrupts.asm -o kernel/interrupts.o

echo "Building kernel entry..."
nasm -f elf32 kernel/entry.asm -o kernel/entry.o

echo "========================================"
echo "           LINKING KERNEL"
echo "========================================"

ld -m elf_i386 -T kernel/linker.ld \
    -o kernel/kernel.elf \
    kernel/entry.o \
    kernel/kernel.o \
    kernel/terminal.o \
    kernel/keyboard.o \
    kernel/shell.o \
    kernel/memory.o \
    kernel/frame.o \
    kernel/paging.o \
    kernel/heap.o \
    hindsight/event.o \
    hindsight/history.o \
    hindsight/incident.o \
    kernel/timer.o \
    kernel/scheduler.o \
    kernel/context.o \
    kernel/task.o \
    kernel/interrupts_c.o \
    kernel/interrupts.o

objcopy -O binary kernel/kernel.elf kernel/kernel.bin

KERNEL_SIZE=$(stat -c%s kernel/kernel.bin)
KERNEL_SECTORS=$(( (KERNEL_SIZE + 511) / 512 ))

echo "Kernel size   : $KERNEL_SIZE bytes"
echo "Kernel sectors: $KERNEL_SECTORS"

if [ "$KERNEL_SECTORS" -gt 64 ]; then
    echo "ERROR: Kernel is too large for current Stage 2 loader."
    exit 1
fi

echo "========================================"
echo "          BUILDING BOOT STAGES"
echo "========================================"

nasm -f bin boot/boot.asm -o boot/boot.bin

nasm -f bin \
    -dKERNEL_SECTORS="$KERNEL_SECTORS" \
    boot/stage2.asm \
    -o boot/stage2.bin

BOOT_SIZE=$(stat -c%s boot/boot.bin)
STAGE2_SIZE=$(stat -c%s boot/stage2.bin)

echo "Boot size     : $BOOT_SIZE bytes"
echo "Stage 2 size  : $STAGE2_SIZE bytes"

if [ "$BOOT_SIZE" -ne 512 ]; then
    echo "ERROR: boot.bin must be exactly 512 bytes."
    exit 1
fi

if [ "$STAGE2_SIZE" -ne 1536 ]; then
    echo "ERROR: stage2.bin must be exactly 1536 bytes."
    exit 1
fi

echo "========================================"
echo "          BUILDING DISK IMAGE"
echo "========================================"

rm -f boot/blackforge.img

dd if=/dev/zero \
   of=boot/blackforge.img \
   bs=512 \
   count=2880 \
   status=none

# Sector 1: Bootloader
dd if=boot/boot.bin \
   of=boot/blackforge.img \
   bs=512 \
   seek=0 \
   conv=notrunc \
   status=none

# Sectors 2-4: Stage 2
dd if=boot/stage2.bin \
   of=boot/blackforge.img \
   bs=512 \
   seek=1 \
   conv=notrunc \
   status=none

# Sector 5+: Kernel
dd if=kernel/kernel.bin \
   of=boot/blackforge.img \
   bs=512 \
   seek=4 \
   conv=notrunc \
   status=none

echo "========================================"
echo "       BLACKFORGE BUILD SUCCESS"
echo "========================================"
echo "Boot size     : $BOOT_SIZE bytes"
echo "Stage 2 size  : $STAGE2_SIZE bytes"
echo "Kernel size   : $KERNEL_SIZE bytes"
echo "Kernel sectors: $KERNEL_SECTORS"
echo "Kernel starts : Disk sector 5"
echo "Kernel loads  : Physical 0xA000"
echo "Image         : boot/blackforge.img"
echo "========================================"
