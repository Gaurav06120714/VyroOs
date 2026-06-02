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
       $(BUILD)/ata.o       \
       $(BUILD)/usb.o       \
       $(BUILD)/speaker.o   \
       $(BUILD)/mouse.o     \
       $(BUILD)/gui.o       \
       $(BUILD)/sha256.o    \
       $(BUILD)/security.o  \
       $(BUILD)/pkg.o       \
       $(BUILD)/smp.o       \
       $(BUILD)/power.o     \
       $(BUILD)/klog.o      \
       $(BUILD)/html.o      \
       $(BUILD)/theme.o     \
       $(BUILD)/compositor.o \
       $(BUILD)/icons.o     \
       $(BUILD)/notify.o    \
       $(BUILD)/widgets.o   \
       $(BUILD)/app.o       \
       $(BUILD)/app_settings.o \
       $(BUILD)/app_files.o    \
       $(BUILD)/app_terminal.o \
       $(BUILD)/app_textedit.o \
       $(BUILD)/app_calc.o     \
       $(BUILD)/app_clock.o    \
       $(BUILD)/app_taskmgr.o  \
       $(BUILD)/app_launcher.o \
       $(BUILD)/app_notecenter.o \
       $(BUILD)/app_control.o  \
       $(BUILD)/app_browser.o  \
       $(BUILD)/app_pkgstore.o \
       $(BUILD)/app_notes.o    \
       $(BUILD)/app_calendar.o \
       $(BUILD)/app_network.o  \
       $(BUILD)/app_disk.o     \
       $(BUILD)/apps_reg.o  \
       $(BUILD)/sockets.o   \
       $(BUILD)/dhcp.o      \
       $(BUILD)/dns.o       \
       $(BUILD)/ipc.o       \
       $(BUILD)/widgets_panel.o \
       $(BUILD)/ctxmenu.o   \
       $(BUILD)/rtl8139.o

$(BUILD)/ctxmenu.o: kernel/ctxmenu.c
	$(CC) $(CFLAGS) kernel/ctxmenu.c -o $(BUILD)/ctxmenu.o

$(BUILD)/rtl8139.o: drivers/rtl8139.c
	$(CC) $(CFLAGS) drivers/rtl8139.c -o $(BUILD)/rtl8139.o

$(BUILD)/sockets.o: kernel/sockets.c
	$(CC) $(CFLAGS) kernel/sockets.c -o $(BUILD)/sockets.o
$(BUILD)/dhcp.o: kernel/dhcp.c
	$(CC) $(CFLAGS) kernel/dhcp.c -o $(BUILD)/dhcp.o
$(BUILD)/dns.o: kernel/dns.c
	$(CC) $(CFLAGS) kernel/dns.c -o $(BUILD)/dns.o
$(BUILD)/ipc.o: kernel/ipc.c
	$(CC) $(CFLAGS) kernel/ipc.c -o $(BUILD)/ipc.o
$(BUILD)/widgets_panel.o: kernel/widgets_panel.c
	$(CC) $(CFLAGS) kernel/widgets_panel.c -o $(BUILD)/widgets_panel.o

# App build rules
$(BUILD)/app_settings.o: kernel/apps/settings.c
	$(CC) $(CFLAGS) kernel/apps/settings.c -o $(BUILD)/app_settings.o
$(BUILD)/app_files.o: kernel/apps/files.c
	$(CC) $(CFLAGS) kernel/apps/files.c -o $(BUILD)/app_files.o
$(BUILD)/app_terminal.o: kernel/apps/terminal.c
	$(CC) $(CFLAGS) kernel/apps/terminal.c -o $(BUILD)/app_terminal.o
$(BUILD)/app_textedit.o: kernel/apps/textedit.c
	$(CC) $(CFLAGS) kernel/apps/textedit.c -o $(BUILD)/app_textedit.o
$(BUILD)/app_calc.o: kernel/apps/calc.c
	$(CC) $(CFLAGS) kernel/apps/calc.c -o $(BUILD)/app_calc.o
$(BUILD)/app_clock.o: kernel/apps/clock.c
	$(CC) $(CFLAGS) kernel/apps/clock.c -o $(BUILD)/app_clock.o
$(BUILD)/app_taskmgr.o: kernel/apps/taskmgr.c
	$(CC) $(CFLAGS) kernel/apps/taskmgr.c -o $(BUILD)/app_taskmgr.o
$(BUILD)/app_launcher.o: kernel/apps/launcher.c
	$(CC) $(CFLAGS) kernel/apps/launcher.c -o $(BUILD)/app_launcher.o
$(BUILD)/app_notecenter.o: kernel/apps/notecenter.c
	$(CC) $(CFLAGS) kernel/apps/notecenter.c -o $(BUILD)/app_notecenter.o
$(BUILD)/app_control.o: kernel/apps/control.c
	$(CC) $(CFLAGS) kernel/apps/control.c -o $(BUILD)/app_control.o
$(BUILD)/app_browser.o: kernel/apps/browser.c
	$(CC) $(CFLAGS) kernel/apps/browser.c -o $(BUILD)/app_browser.o
$(BUILD)/app_pkgstore.o: kernel/apps/pkgstore.c
	$(CC) $(CFLAGS) kernel/apps/pkgstore.c -o $(BUILD)/app_pkgstore.o
$(BUILD)/app_notes.o: kernel/apps/notes.c
	$(CC) $(CFLAGS) kernel/apps/notes.c -o $(BUILD)/app_notes.o
$(BUILD)/app_calendar.o: kernel/apps/calendar.c
	$(CC) $(CFLAGS) kernel/apps/calendar.c -o $(BUILD)/app_calendar.o
$(BUILD)/app_network.o: kernel/apps/network.c
	$(CC) $(CFLAGS) kernel/apps/network.c -o $(BUILD)/app_network.o
$(BUILD)/app_disk.o: kernel/apps/disk.c
	$(CC) $(CFLAGS) kernel/apps/disk.c -o $(BUILD)/app_disk.o
$(BUILD)/apps_reg.o: kernel/apps/apps.c
	$(CC) $(CFLAGS) kernel/apps/apps.c -o $(BUILD)/apps_reg.o

# ───────────────────────────────────────────────
# Default: build + run
# ───────────────────────────────────────────────
all: $(BUILD)/vyro.img $(BUILD)/disk.img
	qemu-system-x86_64 \
		-drive file=$(BUILD)/vyro.img,format=raw,if=ide,index=0,media=disk \
		-drive file=$(BUILD)/disk.img,format=raw,if=ide,index=1,media=disk \
		-m 256M \
		-smp 4 \
		-name "Vyro OS" \
		-display cocoa,show-cursor=on,full-screen=on \
		-vga std \
		-netdev user,id=n0 \
		-device rtl8139,netdev=n0 \
		-device qemu-xhci,id=xhci \
		-audiodev coreaudio,id=snd0 \
		-machine pcspk-audiodev=snd0

build: $(BUILD)/vyro.img

# ───────────────────────────────────────────────
# Disk image
# ───────────────────────────────────────────────
$(BUILD)/vyro.img: $(BUILD)/boot.bin $(BUILD)/kernel.bin
	@mkdir -p $(BUILD)
	dd if=/dev/zero bs=512 count=2880 of=$(BUILD)/vyro.img 2>/dev/null
	dd if=$(BUILD)/boot.bin of=$(BUILD)/vyro.img bs=512 count=1 conv=notrunc 2>/dev/null
	dd if=$(BUILD)/kernel.bin of=$(BUILD)/vyro.img bs=512 seek=1 conv=notrunc 2>/dev/null
	@echo "  [IMG]   vyro.img ready (boot=$(shell wc -c < $(BUILD)/boot.bin)b kernel=$(shell wc -c < $(BUILD)/kernel.bin)b / 196608b max)"
	@if [ $$(wc -c < $(BUILD)/kernel.bin) -gt 196608 ]; then echo "  [WARN]  kernel exceeds 32KB! Increase sector count in boot.asm"; fi

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

$(BUILD)/usb.o: kernel/usb.c
	$(CC) $(CFLAGS) kernel/usb.c -o $(BUILD)/usb.o
	@echo "  [CC]    usb.o"

$(BUILD)/speaker.o: drivers/speaker.c
	$(CC) $(CFLAGS) drivers/speaker.c -o $(BUILD)/speaker.o
	@echo "  [CC]    speaker.o"

$(BUILD)/mouse.o: drivers/mouse.c
	$(CC) $(CFLAGS) drivers/mouse.c -o $(BUILD)/mouse.o
	@echo "  [CC]    mouse.o"

$(BUILD)/gui.o: kernel/gui.c
	$(CC) $(CFLAGS) kernel/gui.c -o $(BUILD)/gui.o
	@echo "  [CC]    gui.o"

$(BUILD)/sha256.o: kernel/sha256.c
	$(CC) $(CFLAGS) kernel/sha256.c -o $(BUILD)/sha256.o
	@echo "  [CC]    sha256.o"

$(BUILD)/security.o: kernel/security.c
	$(CC) $(CFLAGS) kernel/security.c -o $(BUILD)/security.o
	@echo "  [CC]    security.o"

$(BUILD)/pkg.o: kernel/pkg.c
	$(CC) $(CFLAGS) kernel/pkg.c -o $(BUILD)/pkg.o
	@echo "  [CC]    pkg.o"

$(BUILD)/smp.o: kernel/smp.c
	$(CC) $(CFLAGS) kernel/smp.c -o $(BUILD)/smp.o
	@echo "  [CC]    smp.o"

$(BUILD)/power.o: kernel/power.c
	$(CC) $(CFLAGS) kernel/power.c -o $(BUILD)/power.o
	@echo "  [CC]    power.o"

$(BUILD)/klog.o: kernel/klog.c
	$(CC) $(CFLAGS) kernel/klog.c -o $(BUILD)/klog.o
	@echo "  [CC]    klog.o"

$(BUILD)/html.o: kernel/html.c
	$(CC) $(CFLAGS) kernel/html.c -o $(BUILD)/html.o
	@echo "  [CC]    html.o"

$(BUILD)/theme.o: kernel/theme.c
	$(CC) $(CFLAGS) kernel/theme.c -o $(BUILD)/theme.o
	@echo "  [CC]    theme.o"

$(BUILD)/compositor.o: kernel/compositor.c
	$(CC) $(CFLAGS) kernel/compositor.c -o $(BUILD)/compositor.o
	@echo "  [CC]    compositor.o"

$(BUILD)/icons.o: kernel/icons.c
	$(CC) $(CFLAGS) kernel/icons.c -o $(BUILD)/icons.o
	@echo "  [CC]    icons.o"

$(BUILD)/notify.o: kernel/notify.c
	$(CC) $(CFLAGS) kernel/notify.c -o $(BUILD)/notify.o
	@echo "  [CC]    notify.o"

$(BUILD)/widgets.o: kernel/widgets.c
	$(CC) $(CFLAGS) kernel/widgets.c -o $(BUILD)/widgets.o
	@echo "  [CC]    widgets.o"

$(BUILD)/app.o: kernel/app.c
	$(CC) $(CFLAGS) kernel/app.c -o $(BUILD)/app.o
	@echo "  [CC]    app.o"

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
