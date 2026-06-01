# Vyro OS — Makefile
# ==================
# Usage:
#   make       → build and run
#   make clean → remove build artifacts

ASM    = nasm
CC     = gcc
LD     = ld

CFLAGS = -m32 -ffreestanding -fno-stack-protector -nostdlib -c
LFLAGS = -m elf_i386 -T link.ld

all: vyro.bin
	qemu-system-x86_64 -drive format=raw,file=vyro.bin

vyro.bin: boot/boot.bin kernel/kernel.bin
	cat boot/boot.bin kernel/kernel.bin > vyro.bin

boot/boot.bin: boot/boot.asm
	$(ASM) -f bin boot/boot.asm -o boot/boot.bin

kernel/kernel.bin: kernel/kernel_entry.o kernel/kernel.o
	$(LD) $(LFLAGS) -o kernel/kernel.bin kernel/kernel_entry.o kernel/kernel.o

kernel/kernel_entry.o: kernel/kernel_entry.asm
	$(ASM) -f elf kernel/kernel_entry.asm -o kernel/kernel_entry.o

kernel/kernel.o: kernel/kernel.c
	$(CC) $(CFLAGS) kernel/kernel.c -o kernel/kernel.o

clean:
	rm -f boot/*.bin kernel/*.bin kernel/*.o *.bin
