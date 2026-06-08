# LESSON 11-12-13 — Cross Compiler & C Kernel
> Sources:
> - https://github.com/cfenollosa/os-tutorial/tree/master/11-kernel-crosscompiler
> - https://github.com/cfenollosa/os-tutorial/tree/master/12-kernel-c
> - https://github.com/cfenollosa/os-tutorial/tree/master/13-kernel-barebones

## Concepts
cross-compiler, i386-elf-gcc, freestanding C, kernel entry, linker script, Makefile, ELF

---

## LESSON 11 — Build the Cross-Compiler

### Why
The system `gcc` compiles for your host OS (macOS/Linux) and links against the host libc.
Kernel code has NO operating system — you need a compiler that targets bare metal: `i386-elf`.

### Install Required Packages

**macOS:**
```bash
brew install gmp mpfr libmpc gcc
export CC=/usr/local/bin/gcc-$(brew list --versions gcc | awk '{print $2}' | cut -d. -f1)
export LD=$CC
```

**Ubuntu:**
```bash
sudo apt install build-essential bison flex libgmp3-dev libmpc-dev libmpfr-dev texinfo
```

### Build binutils
```bash
export PREFIX="/usr/local/i386elfgcc"
export TARGET=i386-elf
export PATH="$PREFIX/bin:$PATH"

mkdir /tmp/src && cd /tmp/src
curl -O http://ftp.gnu.org/gnu/binutils/binutils-2.36.tar.gz
tar xf binutils-2.36.tar.gz
mkdir binutils-build && cd binutils-build
../binutils-2.36/configure --target=$TARGET --enable-interwork --enable-multilib \
    --disable-nls --disable-werror --prefix=$PREFIX
make all install
```

### Build GCC
```bash
cd /tmp/src
curl -O https://ftp.gnu.org/gnu/gcc/gcc-10.3.0/gcc-10.3.0.tar.gz
tar xf gcc-10.3.0.tar.gz
mkdir gcc-build && cd gcc-build
../gcc-10.3.0/configure --target=$TARGET --prefix="$PREFIX" --disable-nls \
    --disable-libssp --enable-languages=c --without-headers
make all-gcc
make all-target-libgcc
make install-gcc
make install-target-libgcc
```

### Add to PATH (permanently)
```bash
echo 'export PATH="/usr/local/i386elfgcc/bin:$PATH"' >> ~/.zshrc
source ~/.zshrc
i386-elf-gcc --version   # verify
```

---

## LESSON 12 — C for the Kernel

### Compile a C file for bare metal
```bash
i386-elf-gcc -ffreestanding -c function.c -o function.o
```

### Key flags
| Flag | Meaning |
|------|---------|
| `-ffreestanding` | No standard library, no entry point assumptions |
| `-nostdlib` | Don't link any standard library |
| `-m32` | Generate 32-bit code |
| `-c` | Compile only (don't link) |
| `-O0` | Disable optimization (easier to debug) |
| `-g` | Include debug symbols (for GDB) |

### Inspect compiled object
```bash
i386-elf-objdump -d function.o    # disassemble
i386-elf-ld -o function.bin -Ttext 0x0 --oformat binary function.o
ndisasm -b 32 function.bin        # disassemble binary
```

---

## LESSON 13 — First Real Kernel

### File: `kernel/kernel.c`
```c
/* Force compiler to create a non-zero entry point */
void dummy_test_entrypoint() {
}

void main() {
    char *video_memory = (char*) 0xb8000;
    *video_memory = 'X';   /* Print 'X' at top-left of screen */
}
```

### File: `boot/kernel_entry.asm`
```nasm
[bits 32]
[extern main]      ; the C function 'main' in kernel.c
call main
jmp $              ; halt after kernel returns
```

### File: `Makefile`
```makefile
all: run

kernel.bin: kernel_entry.o kernel.o
	i386-elf-ld -o $@ -Ttext 0x1000 $^ --oformat binary

kernel_entry.o: kernel_entry.asm
	nasm $< -f elf -o $@

kernel.o: kernel.c
	i386-elf-gcc -ffreestanding -c $< -o $@

bootsect.bin: bootsect.asm
	nasm $< -f bin -o $@

os-image.bin: bootsect.bin kernel.bin
	cat $^ > $@

run: os-image.bin
	qemu-system-i386 -fda $<

clean:
	rm -f *.bin *.o *.dis
```

### Build & Run
```bash
make        # builds and runs
make clean  # remove all generated files
```

### Why `0x1000`?
The kernel is loaded at memory address `0x1000`. The boot sector reads kernel sectors
from disk and places them at `0x1000`, then jumps to it.

## Common Problems & Solutions

| Problem | Solution |
|---------|----------|
| `undefined reference to main` | `kernel_entry.asm` uses `[extern main]` — ensure `main` exists in `kernel.c` |
| `i386-elf-gcc: command not found` | Cross-compiler not in PATH — add `/usr/local/i386elfgcc/bin` to PATH |
| `ld: cannot find entry symbol _start` | Add `global _start` + `_start:` label in `kernel_entry.asm` (see Lesson 23 fix) |
| Kernel doesn't start | Bootsector must load kernel to correct address and jump to it |
| Nothing on screen | VGA write at `0xB8000` — make sure you're in 32-bit protected mode first |
| `make` fails with "recipe failed" | Check for TABs (not spaces) before each command in Makefile |

## Rules to Remember
- NEVER use host `gcc` for kernel code — always `i386-elf-gcc`
- Compile with `-ffreestanding` — no libc, no entry point
- Link kernel at address `0x1000` with `-Ttext 0x1000`
- `kernel_entry.asm` must call `main` — entry point cannot be C's `main` directly
- `cat bootsect.bin kernel.bin > os-image.bin` — concatenate both into one disk image
