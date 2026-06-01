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
       $(BUILD)/timer.o     \
       $(BUILD)/rtc.o       \
       $(BUILD)/vfs.o       \
       $(BUILD)/switch.o    \
       $(BUILD)/task.o      \
       $(BUILD)/syscall.o   \
       $(BUILD)/gdt.o       \
       $(BUILD)/gdt_flush.o \
       $(BUILD)/usermode.o  \
       $(BUILD)/elf.o       \
       $(BUILD)/user_init.o \
       $(BUILD)/pci.o       \
       $(BUILD)/net.o       \
       $(BUILD)/ata.o

# ───────────────────────────────────────────────
# Default: build + run
# ───────────────────────────────────────────────
all: $(BUILD)/vyro.img $(BUILD)/disk.img
	qemu-system-x86_64 \
		-drive file=$(BUILD)/vyro.img,format=raw,if=ide,index=0,media=disk \
		-drive file=$(BUILD)/disk.img,format=raw,if=ide,index=1,media=disk \
		-m 256M \
		-name "Vyro OS" \
		-display cocoa \
		-vga std \
		-netdev user,id=n0 \
		-device rtl8139,netdev=n0

build: $(BUILD)/vyro.img

# ───────────────────────────────────────────────
# Disk image
# ───────────────────────────────────────────────
$(BUILD)/vyro.img: $(BUILD)/boot.bin $(BUILD)/kernel.bin
	@mkdir -p $(BUILD)
	dd if=/dev/zero bs=512 count=2880 of=$(BUILD)/vyro.img 2>/dev/null
	dd if=$(BUILD)/boot.bin of=$(BUILD)/vyro.img bs=512 count=1 conv=notrunc 2>/dev/null
	dd if=$(BUILD)/kernel.bin of=$(BUILD)/vyro.img bs=512 seek=1 conv=notrunc 2>/dev/null
	@echo "  [IMG]   vyro.img ready (boot=$(shell wc -c < $(BUILD)/boot.bin)b kernel=$(shell wc -c < $(BUILD)/kernel.bin)b / 49152b max)"
	@if [ $$(wc -c < $(BUILD)/kernel.bin) -gt 49152 ]; then echo "  [WARN]  kernel exceeds 32KB! Increase sector count in boot.asm"; fi

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

$(BUILD)/switch.o: kernel/switch.asm
	$(ASM) -f elf64 kernel/switch.asm -o $(BUILD)/switch.o
	@echo "  [ASM]   switch.o"

$(BUILD)/gdt_flush.o: kernel/gdt_flush.asm
	$(ASM) -f elf64 kernel/gdt_flush.asm -o $(BUILD)/gdt_flush.o
	@echo "  [ASM]   gdt_flush.o"

$(BUILD)/usermode.o: kernel/usermode.asm
	$(ASM) -f elf64 kernel/usermode.asm -o $(BUILD)/usermode.o
	@echo "  [ASM]   usermode.o"

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

$(BUILD)/rtc.o: drivers/rtc.c
	$(CC) $(CFLAGS) drivers/rtc.c -o $(BUILD)/rtc.o
	@echo "  [CC]    rtc.o"

$(BUILD)/vfs.o: kernel/vfs.c
	$(CC) $(CFLAGS) kernel/vfs.c -o $(BUILD)/vfs.o
	@echo "  [CC]    vfs.o"

$(BUILD)/task.o: kernel/task.c
	$(CC) $(CFLAGS) kernel/task.c -o $(BUILD)/task.o
	@echo "  [CC]    task.o"

$(BUILD)/syscall.o: kernel/syscall.c
	$(CC) $(CFLAGS) kernel/syscall.c -o $(BUILD)/syscall.o
	@echo "  [CC]    syscall.o"

$(BUILD)/gdt.o: kernel/gdt.c
	$(CC) $(CFLAGS) kernel/gdt.c -o $(BUILD)/gdt.o
	@echo "  [CC]    gdt.o"

$(BUILD)/elf.o: kernel/elf.c
	$(CC) $(CFLAGS) kernel/elf.c -o $(BUILD)/elf.o
	@echo "  [CC]    elf.o"

$(BUILD)/pci.o: kernel/pci.c
	$(CC) $(CFLAGS) kernel/pci.c -o $(BUILD)/pci.o
	@echo "  [CC]    pci.o"

$(BUILD)/net.o: kernel/net.c
	$(CC) $(CFLAGS) kernel/net.c -o $(BUILD)/net.o
	@echo "  [CC]    net.o"

$(BUILD)/ata.o: kernel/ata.c
	$(CC) $(CFLAGS) kernel/ata.c -o $(BUILD)/ata.o
	@echo "  [CC]    ata.o"

# Scratch disk for ATA driver (persists between runs, NOT recreated by clean)
$(BUILD)/disk.img:
	@mkdir -p $(BUILD)
	dd if=/dev/zero of=$(BUILD)/disk.img bs=512 count=2048 2>/dev/null
	@echo "  [DISK]  disk.img created (1MB scratch)"

# ───────────────────────────────────────────────
# User ELF program → embedded as a C byte array
# ───────────────────────────────────────────────
$(BUILD)/init.elf: user/init.c user/user.ld
	@mkdir -p $(BUILD)
	$(CC) -ffreestanding -fno-stack-protector -fno-builtin -nostdlib \
	      -nostdinc -mno-red-zone -mno-sse -mno-sse2 -mno-mmx -mno-80387 \
	      -O2 -c user/init.c -o $(BUILD)/init_user.o
	$(LD) -T user/user.ld -o $(BUILD)/init.elf $(BUILD)/init_user.o
	@echo "  [USER]  init.elf"

$(BUILD)/user_init.c: $(BUILD)/init.elf
	@python3 -c "import sys; d=open('$(BUILD)/init.elf','rb').read(); \
open('$(BUILD)/user_init.c','w').write('const unsigned char user_init_elf[]={'+','.join(str(b) for b in d)+'};\nconst unsigned int user_init_elf_len='+str(len(d))+';\n')"
	@echo "  [GEN]   user_init.c ($(shell wc -c < $(BUILD)/init.elf 2>/dev/null || echo 0) bytes ELF)"

$(BUILD)/user_init.o: $(BUILD)/user_init.c
	$(CC) $(CFLAGS) $(BUILD)/user_init.c -o $(BUILD)/user_init.o
	@echo "  [CC]    user_init.o"

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
