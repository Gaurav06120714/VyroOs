# LESSON 00 — Environment Setup
> Source: https://github.com/cfenollosa/os-tutorial/tree/master/00-environment

## Goal
Install every tool needed to build and run your OS from scratch.

## Tools to Install

| Tool | Why |
|------|-----|
| `nasm` | Assembles x86 `.asm` files into machine code |
| `qemu` | Emulates a PC — run your OS without real hardware |
| `i386-elf-gcc` | Cross-compiler — builds C code for bare metal (no host OS) |
| `i386-elf-ld` | Linker for bare metal binaries |
| `make` | Automates the build |
| `gdb` (i386-elf) | Debugger — step through kernel code live |

## Install Commands

### macOS
```bash
brew install nasm qemu
# Cross-compiler must be built manually (see Lesson 11)
# DO NOT use Apple's built-in nasm — it won't work
export PATH="/usr/local/bin:$PATH"  # ensure Homebrew nasm comes first
```

### Ubuntu / Debian
```bash
sudo apt update
sudo apt install nasm qemu-system-x86 build-essential gdb
# Cross-compiler: also build manually (see Lesson 11)
```

## Verify Install
```bash
nasm --version        # should say 2.x or higher
qemu-system-i386 --version
i386-elf-gcc --version   # after building cross-compiler
```

## Common Problems & Solutions

| Problem | Solution |
|---------|----------|
| `nasm: command not found` | `brew install nasm` or `apt install nasm` |
| QEMU shows SDL error | Add `--nographic` or `--curses` flag |
| `nasm` gives wrong output on macOS | Use `/usr/local/bin/nasm`, not `/usr/bin/nasm` (Xcode version) |
| `i386-elf-gcc` not found | You need to build the cross-compiler — see Lesson 11 |
