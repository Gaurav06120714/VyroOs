# VyroOS — OS Build Rules & Step Guide

Sources:
- https://github.com/cfenollosa/os-tutorial  (step-by-step lessons 00–23)
- https://github.com/torvalds/linux           (production kernel reference)

---

## File Index

| File | What's Inside |
|------|---------------|
| [00-environment.md](00-environment.md) | Install nasm, qemu, cross-compiler |
| [01-bootsector-barebones.md](01-bootsector-barebones.md) | Smallest bootable 512-byte sector |
| [02-bootsector-print.md](02-bootsector-print.md) | Print text via BIOS int 0x10 |
| [03-bootsector-memory.md](03-bootsector-memory.md) | Memory layout, org 0x7C00 |
| [04-bootsector-stack.md](04-bootsector-stack.md) | Stack setup, push/pop/call/ret |
| [05-bootsector-functions-strings.md](05-bootsector-functions-strings.md) | Functions, strings, %include |
| [06-bootsector-segmentation.md](06-bootsector-segmentation.md) | Real mode segment:offset addressing |
| [07-bootsector-disk.md](07-bootsector-disk.md) | Load kernel from disk via int 0x13 |
| [08-09-10-protected-mode-gdt.md](08-09-10-protected-mode-gdt.md) | VGA direct write, GDT, switch to 32-bit |
| [11-12-13-c-kernel.md](11-12-13-c-kernel.md) | Cross-compiler, freestanding C, first kernel |
| [14-checkpoint-debug.md](14-checkpoint-debug.md) | Folder structure, Makefile, GDB |
| [15-16-17-vga-driver.md](15-16-17-vga-driver.md) | I/O ports, VGA driver, kprint, scroll |
| [18-19-20-interrupts.md](18-19-20-interrupts.md) | IDT, ISRs, PIC, timer, keyboard |
| [21-22-23-shell-malloc-fixes.md](21-22-23-shell-malloc-fixes.md) | Shell, kmalloc, all bug fixes |
| [advanced-linux-kernel.md](advanced-linux-kernel.md) | Paging, scheduling, syscalls, VFS, user mode |

---

## Quick Build Reference

```bash
# Every lesson
nasm -f bin boot.asm -o boot.bin
qemu-system-i386 -fda boot.bin

# From Lesson 13 onwards
make          # build + run
make debug    # build + launch with GDB
make clean    # remove all generated files
```

## Golden Rules

1. Go in order — every lesson builds on the previous
2. Test in QEMU first — never on real hardware until it works
3. Cross-compile always — never use system gcc for kernel code
4. No libc — you write your own malloc, memcpy, printf
5. Interrupts first — get IDT working before anything advanced
6. One feature at a time — build, boot, verify, continue
7. When stuck on advanced topics — read Linux kernel source + docs
