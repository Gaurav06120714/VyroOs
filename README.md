# Vyro OS

> A custom operating system built from scratch in Assembly and C.
> Boots on QEMU with zero dependencies. $0 budget. 100% raw.

---

## Tech Stack

| Tool | Purpose |
|------|---------|
| NASM | Assembly compiler — bootloader & low-level code |
| GCC | C compiler — kernel, drivers, shell |
| QEMU | Emulator — run Vyro OS on Mac & Windows |
| GNU Make | Build automation |
| GNU LD | Linker — connects ASM and C into one binary |
| Git | Version control |

---

## Project Structure

```
VyroOs/
├── boot/
│   └── boot.asm          ← Bootloader (first code that runs)
├── kernel/
│   ├── kernel.c          ← Main kernel logic
│   └── kernel_entry.asm  ← Bridges bootloader → kernel
├── drivers/
│   ├── screen.c          ← Text output driver
│   └── keyboard.c        ← Keyboard input driver
├── include/
│   └── types.h           ← Shared type definitions
├── link.ld               ← Linker script
├── Makefile              ← Build & run automation
└── README.md
```

---

## Setup (Mac & Windows)

### Mac
```bash
brew install qemu nasm gcc make
```

### Windows (WSL)
```bash
# Step 1 — Open PowerShell as Admin
wsl --install

# Step 2 — Inside WSL
sudo apt install qemu nasm gcc make build-essential
```

---

## Build & Run

```bash
make        # Build and launch Vyro OS in QEMU
make clean  # Remove build artifacts
```

---

## Build Progress

- [x] Phase 0 — Project structure & tooling setup
- [x] Phase 1 — Bootloader (print "VYRO OS" on screen)
- [x] Phase 2 — 64-bit Long Mode (16-bit → 32-bit → 64-bit)
- [x] Phase 3 — C kernel (screen driver, kernel_main, boot status display)
- [x] Phase 4 — Interrupt system (IDT, ISR, IRQ, PIC remapping)
- [x] Phase 5 — Keyboard driver (PS/2, scan codes, shift, caps lock, shell)
- [ ] Phase 6 — Interactive shell (commands)
- [ ] Phase 7 — Memory management
- [ ] Phase 8 — File system
- [ ] Phase 9 — GUI (long term)

---

## License

MIT — free to use, modify, and build on.
