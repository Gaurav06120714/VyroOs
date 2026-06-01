# ═══════════════════════════════════════════════
# VYRO OS — Build System
# ═══════════════════════════════════════════════
# Usage:
#   make          → build disk image + launch QEMU
#   make build    → build only (no QEMU)
#   make clean    → remove all build artifacts
#   make debug    → launch with GDB server on :1234

ASM     = nasm
CC      = x86_64-elf-gcc
LD      = x86_64-elf-ld

CFLAGS  = -ffreestanding \
          -fno-stack-protector \
          -fno-builtin \
          -nostdlib \
          -nostdinc \
          -mno-red-zone \
          -O2 \
          -Wall \
          -Wextra \
          -c

LDFLAGS = -T link.ld \
          --oformat binary

BUILD   = build

# ───────────────────────────────────────────────
# Default target: build and run
# ───────────────────────────────────────────────
all: $(BUILD)/vyro.img
	qemu-system-x86_64 \
		-drive format=raw,file=$(BUILD)/vyro.img \
		-m 256M \
		-name "Vyro OS"

# ───────────────────────────────────────────────
# Build only (no QEMU)
# ───────────────────────────────────────────────
build: $(BUILD)/vyro.img

# ───────────────────────────────────────────────
# Disk image: bootloader + kernel padded to 1.44MB
# ───────────────────────────────────────────────
$(BUILD)/vyro.img: $(BUILD)/boot.bin $(BUILD)/kernel.bin
	@mkdir -p $(BUILD)
	dd if=/dev/zero bs=512 count=2880 of=$(BUILD)/vyro.img 2>/dev/null
	dd if=$(BUILD)/boot.bin of=$(BUILD)/vyro.img conv=notrunc 2>/dev/null
	dd if=$(BUILD)/kernel.bin of=$(BUILD)/vyro.img seek=1 conv=notrunc 2>/dev/null
	@echo "  [BUILD] vyro.img ready ($(shell wc -c < $(BUILD)/kernel.bin) bytes kernel)"

# ───────────────────────────────────────────────
# Bootloader
# ───────────────────────────────────────────────
$(BUILD)/boot.bin: boot/boot.asm
	@mkdir -p $(BUILD)
	$(ASM) -f bin boot/boot.asm -o $(BUILD)/boot.bin
	@echo "  [ASM]   boot.bin"

# ───────────────────────────────────────────────
# Kernel binary (linked flat binary at 0x10000)
# ───────────────────────────────────────────────
$(BUILD)/kernel.bin: $(BUILD)/kernel_entry.o $(BUILD)/kernel.o $(BUILD)/screen.o
	$(LD) $(LDFLAGS) -o $(BUILD)/kernel.bin \
		$(BUILD)/kernel_entry.o \
		$(BUILD)/kernel.o \
		$(BUILD)/screen.o
	@echo "  [LINK]  kernel.bin"

# ───────────────────────────────────────────────
# Object files
# ───────────────────────────────────────────────
$(BUILD)/kernel_entry.o: kernel/kernel_entry.asm
	@mkdir -p $(BUILD)
	$(ASM) -f elf64 kernel/kernel_entry.asm -o $(BUILD)/kernel_entry.o
	@echo "  [ASM]   kernel_entry.o"

$(BUILD)/kernel.o: kernel/kernel.c
	$(CC) $(CFLAGS) kernel/kernel.c -o $(BUILD)/kernel.o
	@echo "  [CC]    kernel.o"

$(BUILD)/screen.o: drivers/screen.c
	$(CC) $(CFLAGS) drivers/screen.c -o $(BUILD)/screen.o
	@echo "  [CC]    screen.o"

# ───────────────────────────────────────────────
# Debug: launch QEMU with GDB server
# ───────────────────────────────────────────────
debug: $(BUILD)/vyro.img
	qemu-system-x86_64 \
		-drive format=raw,file=$(BUILD)/vyro.img \
		-m 256M \
		-s -S \
		-name "Vyro OS [DEBUG]"

# ───────────────────────────────────────────────
# Clean build artifacts
# ───────────────────────────────────────────────
clean:
	rm -rf $(BUILD)/
	@echo "  [CLEAN] build directory removed"

.PHONY: all build debug clean
