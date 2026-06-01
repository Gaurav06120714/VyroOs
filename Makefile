# ═══════════════════════════════════════════════
# VYRO OS — Build System
# ═══════════════════════════════════════════════
ASM     = nasm
CC      = x86_64-elf-gcc
LD      = x86_64-elf-ld

CFLAGS  = -ffreestanding \
          -fno-stack-protector \
          -fno-builtin \
          -nostdlib \
          -nostdinc \
          -mno-red-zone \
          -mno-sse \
          -mno-sse2 \
          -mno-mmx \
          -mno-80387 \
          -O2 \
          -Wall \
          -Wextra \
          -c

LDFLAGS = -T link.ld \
          --oformat binary

BUILD   = build

# ───────────────────────────────────────────────
# All object files
# ───────────────────────────────────────────────
OBJS = $(BUILD)/kernel_entry.o \
       $(BUILD)/isr_stubs.o    \
       $(BUILD)/kernel.o       \
       $(BUILD)/idt.o          \
       $(BUILD)/isr.o          \
       $(BUILD)/screen.o       \
       $(BUILD)/framebuffer.o  \
       $(BUILD)/pic.o          \
       $(BUILD)/keyboard.o  \
       $(BUILD)/shell.o     \
       $(BUILD)/pmm.o       \
       $(BUILD)/heap.o      \
       $(BUILD)/timer.o

# ───────────────────────────────────────────────
# Default: build + run
# ───────────────────────────────────────────────
all: $(BUILD)/vyro.img
	qemu-system-x86_64 \
		-drive format=raw,file=$(BUILD)/vyro.img \
		-m 256M \
		-name "Vyro OS" \
		-display cocoa \
		-vga std

build: $(BUILD)/vyro.img

# ───────────────────────────────────────────────
# Disk image
# ───────────────────────────────────────────────
$(BUILD)/vyro.img: $(BUILD)/boot.bin $(BUILD)/kernel.bin
	@mkdir -p $(BUILD)
	dd if=/dev/zero bs=512 count=2880 of=$(BUILD)/vyro.img 2>/dev/null
	dd if=$(BUILD)/boot.bin of=$(BUILD)/vyro.img bs=512 count=1 conv=notrunc 2>/dev/null
	dd if=$(BUILD)/kernel.bin of=$(BUILD)/vyro.img bs=512 seek=1 conv=notrunc 2>/dev/null
	@echo "  [IMG]   vyro.img ready (boot=$(shell wc -c < $(BUILD)/boot.bin)b kernel=$(shell wc -c < $(BUILD)/kernel.bin)b / 32768b max)"
	@if [ $$(wc -c < $(BUILD)/kernel.bin) -gt 32768 ]; then echo "  [WARN]  kernel exceeds 32KB! Increase sector count in boot.asm"; fi

# ───────────────────────────────────────────────
# Bootloader
# ───────────────────────────────────────────────
$(BUILD)/boot.bin: boot/boot.asm
	@mkdir -p $(BUILD)
	$(ASM) -f bin boot/boot.asm -o $(BUILD)/boot.bin
	@echo "  [ASM]   boot.bin"

# ───────────────────────────────────────────────
# Kernel binary
# ───────────────────────────────────────────────
$(BUILD)/kernel.bin: $(OBJS)
	$(LD) $(LDFLAGS) -o $(BUILD)/kernel.bin $(OBJS)
	@echo "  [LINK]  kernel.bin"

# ───────────────────────────────────────────────
# Assembly objects
# ───────────────────────────────────────────────
$(BUILD)/kernel_entry.o: kernel/kernel_entry.asm
	@mkdir -p $(BUILD)
	$(ASM) -f elf64 kernel/kernel_entry.asm -o $(BUILD)/kernel_entry.o
	@echo "  [ASM]   kernel_entry.o"

$(BUILD)/isr_stubs.o: kernel/isr_stubs.asm
	$(ASM) -f elf64 kernel/isr_stubs.asm -o $(BUILD)/isr_stubs.o
	@echo "  [ASM]   isr_stubs.o"

# ───────────────────────────────────────────────
# C objects
# ───────────────────────────────────────────────
$(BUILD)/kernel.o: kernel/kernel.c
	$(CC) $(CFLAGS) kernel/kernel.c -o $(BUILD)/kernel.o
	@echo "  [CC]    kernel.o"

$(BUILD)/idt.o: kernel/idt.c
	$(CC) $(CFLAGS) kernel/idt.c -o $(BUILD)/idt.o
	@echo "  [CC]    idt.o"

$(BUILD)/isr.o: kernel/isr.c
	$(CC) $(CFLAGS) kernel/isr.c -o $(BUILD)/isr.o
	@echo "  [CC]    isr.o"

$(BUILD)/screen.o: drivers/screen.c
	$(CC) $(CFLAGS) drivers/screen.c -o $(BUILD)/screen.o
	@echo "  [CC]    screen.o"

$(BUILD)/framebuffer.o: drivers/framebuffer.c
	$(CC) $(CFLAGS) drivers/framebuffer.c -o $(BUILD)/framebuffer.o
	@echo "  [CC]    framebuffer.o"

$(BUILD)/pic.o: drivers/pic.c
	$(CC) $(CFLAGS) drivers/pic.c -o $(BUILD)/pic.o
	@echo "  [CC]    pic.o"

$(BUILD)/keyboard.o: drivers/keyboard.c
	$(CC) $(CFLAGS) drivers/keyboard.c -o $(BUILD)/keyboard.o
	@echo "  [CC]    keyboard.o"

$(BUILD)/shell.o: kernel/shell.c
	$(CC) $(CFLAGS) kernel/shell.c -o $(BUILD)/shell.o
	@echo "  [CC]    shell.o"

$(BUILD)/pmm.o: kernel/pmm.c
	$(CC) $(CFLAGS) kernel/pmm.c -o $(BUILD)/pmm.o
	@echo "  [CC]    pmm.o"

$(BUILD)/heap.o: kernel/heap.c
	$(CC) $(CFLAGS) kernel/heap.c -o $(BUILD)/heap.o
	@echo "  [CC]    heap.o"

$(BUILD)/timer.o: drivers/timer.c
	$(CC) $(CFLAGS) drivers/timer.c -o $(BUILD)/timer.o
	@echo "  [CC]    timer.o"

# ───────────────────────────────────────────────
# Debug with GDB
# ───────────────────────────────────────────────
debug: $(BUILD)/vyro.img
	qemu-system-x86_64 \
		-drive format=raw,file=$(BUILD)/vyro.img \
		-m 256M -s -S \
		-name "Vyro OS [DEBUG]"

clean:
	rm -rf $(BUILD)/
	@echo "  [CLEAN] done"

.PHONY: all build debug clean
