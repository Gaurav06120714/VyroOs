# Vyro OS — Makefile
# ==================
# Usage:
#   make       → build and run
#   make clean → remove build artifacts

ASM = nasm

all: vyro.bin
	qemu-system-x86_64 -drive format=raw,file=vyro.bin

vyro.bin: boot/boot.asm
	$(ASM) -f bin boot/boot.asm -o vyro.bin

clean:
	rm -f *.bin
