# LESSON 14 — Checkpoint: Code Organization & GDB Debugging
> Source: https://github.com/cfenollosa/os-tutorial/tree/master/14-checkpoint

## Concepts
monolithic kernel, microkernel, GDB debugger, folder structure, scalable Makefile

## Goal
Organize code into folders. Debug the kernel live with GDB + QEMU.

---

## Recommended Folder Structure
```
VyroOs/
├── boot/
│   ├── bootsect.asm        # boot sector
│   └── kernel_entry.asm    # asm entry point → calls kernel main
├── kernel/
│   ├── kernel.c            # kernel main
│   └── util.c              # int_to_ascii, etc.
├── drivers/
│   ├── screen.c            # VGA text driver
│   ├── screen.h
│   ├── keyboard.c
│   └── keyboard.h
├── cpu/
│   ├── idt.c               # Interrupt Descriptor Table
│   ├── idt.h
│   ├── isr.c               # Interrupt Service Routines
│   ├── isr.h
│   └── interrupt.asm       # low-level ISR stubs
├── libc/
│   ├── mem.c               # memory functions + kmalloc
│   ├── mem.h
│   ├── string.c            # strlen, strcmp, etc.
│   └── string.h
├── Makefile
└── link.ld                 # linker script (optional)
```

---

## Build Cross-Compiled GDB (macOS)
```bash
cd /tmp/src
curl -O http://ftp.gnu.org/gnu/gdb/gdb-10.1.tar.gz
tar xf gdb-10.1.tar.gz
mkdir gdb-build && cd gdb-build
export PREFIX="/usr/local/i386elfgcc"
export TARGET=i386-elf
../gdb-10.1/configure --target="$TARGET" --prefix="$PREFIX" --program-prefix=i386-elf-
make
make install
```

---

## Scalable Makefile
```makefile
C_SOURCES = $(wildcard kernel/*.c drivers/*.c cpu/*.c libc/*.c)
HEADERS   = $(wildcard kernel/*.h drivers/*.h cpu/*.h libc/*.h)
OBJ       = ${C_SOURCES:.c=.o cpu/interrupt.o}

CFLAGS = -m32 -g -ffreestanding -Wall -Wextra -fno-pie -nostdlib

all: os-image.bin

os-image.bin: boot/bootsect.bin kernel.bin
	cat $^ > $@

kernel.bin: boot/kernel_entry.o ${OBJ}
	i386-elf-ld -m elf_i386 -o $@ -Ttext 0x1000 $^ --oformat binary

kernel.elf: boot/kernel_entry.o ${OBJ}
	i386-elf-ld -m elf_i386 -o $@ -Ttext 0x1000 $^

run: os-image.bin
	qemu-system-i386 -fda $<

debug: os-image.bin kernel.elf
	qemu-system-i386 -fda os-image.bin -s -S &
	i386-elf-gdb kernel.elf \
		-ex "target remote localhost:1234" \
		-ex "set architecture i386"

%.o: %.c ${HEADERS}
	i386-elf-gcc ${CFLAGS} -c $< -o $@

%.o: %.asm
	nasm $< -f elf32 -o $@

boot/%.bin: boot/%.asm
	nasm $< -f bin -o $@

clean:
	rm -f *.bin *.elf **/*.o **/*.bin
```

---

## Debugging with GDB

### Step 1 — Start QEMU in debug mode
```bash
make debug
# OR manually:
qemu-system-i386 -fda os-image.bin -s -S &
# -s = open gdbserver on port 1234
# -S = freeze CPU at startup, wait for GDB
```

### Step 2 — Connect GDB
```bash
i386-elf-gdb kernel.elf
(gdb) target remote localhost:1234
(gdb) set architecture i386
```

### Useful GDB Commands
```
break kernel_main       # set breakpoint at function
break kernel.c:21       # set breakpoint at line 21
continue                # run until breakpoint
next                    # step over (one C line)
step                    # step into function
print video_memory      # print variable value
print *video_memory     # print what pointer points to
info registers          # show all CPU registers
x/10x 0xb8000           # examine 10 hex words at VGA memory
x/s 0x1000              # examine as string at address
layout src              # show source code pane
quit                    # exit GDB
```

## Common Problems & Solutions

| Problem | Solution |
|---------|----------|
| GDB says "not in executable format" | You need `kernel.elf` (not `.bin`) for GDB symbols |
| Can't connect to port 1234 | QEMU must be running with `-s` flag first |
| Breakpoint never hits | Check function name matches exactly; check that kernel.elf is up to date |
| `info registers` shows 16-bit | Set architecture: `set architecture i386` |
| QEMU closes before GDB connects | Start QEMU with `-S` (freeze at start) — it waits for GDB |

## Rules to Remember
- `kernel.elf` = debug binary (has symbols) — for GDB only
- `kernel.bin` = raw binary — what actually runs
- `make debug` builds both and launches QEMU waiting for GDB
- Add `-g` to CFLAGS for full debug symbols
- Use `strings kernel.elf` to see all string literals in the kernel
