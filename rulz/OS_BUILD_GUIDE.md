# VyroOS — Master OS Build Guide

> Sources:
> - Step-by-step lessons: https://github.com/cfenollosa/os-tutorial
> - Production reference kernel: https://github.com/torvalds/linux

---

## PHASE 0 — Environment Setup

**Source:** `00-environment` | [os-tutorial](https://github.com/cfenollosa/os-tutorial/tree/master/00-environment)

### Tools Required
| Tool | Purpose |
|------|---------|
| `nasm` | x86 assembler |
| `qemu` | PC emulator for testing |
| `gcc` (cross-compiler) | C compiler targeting i686-elf |
| `ld` | GNU linker |
| `make` | Build automation |
| `gdb` | Kernel debugger |

### Install (macOS)
```bash
brew install qemu nasm
# Cross-compiler (i686-elf-gcc) must be built from source — see Lesson 11
```

### Install (Linux/Ubuntu)
```bash
sudo apt install nasm qemu-system-x86 build-essential gdb
```

---

## PHASE 1 — Boot Sector (x86 Assembly)

> The BIOS loads the first 512 bytes of disk into memory at `0x7C00` and executes it.
> Your job: make those 512 bytes valid, mark them with `0xAA55` magic bytes.

---

### LESSON 01 — Bare Boot Sector
**Source:** [01-bootsector-barebones](https://github.com/cfenollosa/os-tutorial/tree/master/01-bootsector-barebones)

**Goal:** Create a file the BIOS treats as a bootable disk.

**Rules:**
- Boot sector MUST be exactly 512 bytes
- Last two bytes MUST be `0x55` and `0xAA` (magic signature)
- BIOS loads it at memory address `0x7C00`

```asm
; boot.asm
loop:
    jmp loop          ; infinite loop — do nothing for now

times 510-($-$$) db 0 ; pad to 510 bytes
dw 0xAA55             ; magic boot signature
```

```bash
nasm -f bin boot.asm -o boot.bin
qemu-system-x86_64 -fda boot.bin
```

---

### LESSON 02 — Print Text via BIOS Interrupt
**Source:** [02-bootsector-print](https://github.com/cfenollosa/os-tutorial/tree/master/02-bootsector-print)

**Goal:** Print characters using BIOS interrupt `0x10`.

**Rules:**
- Use `int 0x10` with `ah=0x0e` for teletype output
- Load character into `al`, then call interrupt
- This only works in 16-bit real mode

```asm
mov ah, 0x0e
mov al, 'H'
int 0x10
```

---

### LESSON 03 — Memory Organization
**Source:** [03-bootsector-memory](https://github.com/cfenollosa/os-tutorial/tree/master/03-bootsector-memory)

**Goal:** Understand x86 memory layout at boot.

**Rules:**
- Boot sector loads at `0x7C00`
- Use `[org 0x7C00]` to tell NASM the base address
- Memory before `0x7C00` is used by BIOS — don't touch it
- Real mode memory map:
  - `0x00000–0x003FF` — Interrupt Vector Table
  - `0x00400–0x004FF` — BIOS Data Area
  - `0x07C00–0x07DFF` — Your boot sector (512 bytes)
  - `0x07E00+` — Free to use

---

### LESSON 04 — The Stack
**Source:** [04-bootsector-stack](https://github.com/cfenollosa/os-tutorial/tree/master/04-bootsector-stack)

**Goal:** Learn to use the x86 stack.

**Rules:**
- `bp` = base of stack, `sp` = top (grows downward)
- Set stack safely above `0x8000` to avoid collision with boot sector
- Use `push` / `pop` / `call` / `ret`

```asm
mov bp, 0x9000
mov sp, bp
push 'A'
pop bx       ; bx = 'A'
```

---

### LESSON 05 — Functions & Strings
**Source:** [05-bootsector-functions-strings](https://github.com/cfenollosa/os-tutorial/tree/master/05-bootsector-functions-strings)

**Goal:** Write loops, call functions, handle null-terminated strings.

**Rules:**
- Strings end with `0x00` (null terminator)
- Use `call label` / `ret` for function calls
- Loop with `jnz` / `jmp` after comparing registers

---

### LESSON 06 — Segmentation (Real Mode)
**Source:** [06-bootsector-segmentation](https://github.com/cfenollosa/os-tutorial/tree/master/06-bootsector-segmentation)

**Goal:** Address memory using segment:offset notation.

**Rules:**
- Real mode address = `segment * 16 + offset`
- Segment registers: `CS` (code), `DS` (data), `SS` (stack), `ES` (extra)
- Be careful: changing `DS` affects all data references

---

### LESSON 07 — Load from Disk
**Source:** [07-bootsector-disk](https://github.com/cfenollosa/os-tutorial/tree/master/07-bootsector-disk)

**Goal:** Read additional sectors from disk using BIOS interrupt `0x13`.

**Rules:**
- BIOS `int 0x13`, `ah=0x02` reads sectors from disk
- Sectors are 1-indexed; sector 1 = boot sector
- Load kernel code into memory starting at `0x1000` or `0x8000`
- Always check the carry flag after disk read for errors

```asm
mov ah, 0x02   ; read sectors
mov al, 2      ; number of sectors
mov ch, 0      ; cylinder
mov cl, 2      ; sector (1-indexed, starts after boot sector)
mov dh, 0      ; head
int 0x13
jc disk_error
```

---

## PHASE 2 — Enter 32-bit Protected Mode

---

### LESSON 08 — Print in Protected Mode
**Source:** [08-32bit-print](https://github.com/cfenollosa/os-tutorial/tree/master/08-32bit-print)

**Goal:** Write directly to VGA video memory at `0xB8000`.

**Rules:**
- In 32-bit mode: NO BIOS interrupts
- VGA text memory starts at `0xB8000`
- Each character = 2 bytes: `[char][attribute]`
- Attribute byte: high nibble = background color, low nibble = foreground

```c
char *video = (char*) 0xB8000;
video[0] = 'H';
video[1] = 0x07;  // light grey on black
```

---

### LESSON 09 — Global Descriptor Table (GDT)
**Source:** [09-32bit-gdt](https://github.com/cfenollosa/os-tutorial/tree/master/09-32bit-gdt)

**Goal:** Define the GDT — required before switching to protected mode.

**Rules:**
- GDT defines memory segments with base, limit, and flags
- Minimum: null descriptor + code segment + data segment
- Use `lgdt` instruction to load GDT register
- Code segment: base=0, limit=0xFFFFF, type=execute/read
- Data segment: base=0, limit=0xFFFFF, type=read/write

---

### LESSON 10 — Switch to 32-bit Protected Mode
**Source:** [10-32bit-enter](https://github.com/cfenollosa/os-tutorial/tree/master/10-32bit-enter)

**Goal:** Actually enter protected mode.

**Rules (strict order):**
1. Disable interrupts (`cli`)
2. Load the GDT (`lgdt`)
3. Set bit 0 of `CR0` register
4. Far jump to flush the CPU pipeline
5. Update all segment registers to point to data descriptor
6. Set up stack pointer
7. You are now in 32-bit protected mode

```asm
cli
lgdt [gdt_descriptor]
mov eax, cr0
or eax, 0x1
mov cr0, eax
jmp CODE_SEG:init_pm   ; far jump — flushes pipeline
```

---

## PHASE 3 — C Kernel

---

### LESSON 11 — Cross-Compiler Setup
**Source:** [11-kernel-crosscompiler](https://github.com/cfenollosa/os-tutorial/tree/master/11-kernel-crosscompiler)

**Goal:** Build a cross-compiler targeting bare metal (i686-elf).

**Rules:**
- NEVER use the host system's `gcc` for kernel code — it links against host libc
- Target: `i686-elf`
- Build order: binutils first, then gcc
- Required env var: `PREFIX=/usr/local/i686-elfgcc`

```bash
# Set target
export TARGET=i686-elf
export PREFIX="$HOME/opt/cross"
export PATH="$PREFIX/bin:$PATH"

# Build binutils then GCC targeting i686-elf
```

---

### LESSON 12 — C in the Kernel
**Source:** [12-kernel-c](https://github.com/cfenollosa/os-tutorial/tree/master/12-kernel-c)

**Goal:** Write low-level code in C instead of Assembly.

**Rules:**
- Compile with `-ffreestanding` flag (no standard library)
- Link with `-nostdlib`
- Use `-m32` for 32-bit output
- Do NOT use `printf`, `malloc`, etc — they depend on an OS you haven't written yet

```bash
i686-elf-gcc -ffreestanding -c kernel.c -o kernel.o
i686-elf-ld -o kernel.bin -Ttext 0x1000 kernel.o --oformat binary
```

---

### LESSON 13 — First Real Kernel
**Source:** [13-kernel-barebones](https://github.com/cfenollosa/os-tutorial/tree/master/13-kernel-barebones)

**Goal:** Create a boot sector that loads and jumps to a C kernel.

**Rules:**
- Kernel entry point must be a C function (e.g., `kernel_main`)
- Bootsector loads kernel sectors from disk into memory
- Use a linker script to control layout
- Jump to kernel entry address after loading

```c
// kernel.c
void kernel_main() {
    // write 'X' to top-left of VGA
    char *video_memory = (char*) 0xb8000;
    *video_memory = 'X';
}
```

---

### LESSON 14 — Checkpoint: Organize + Debug with GDB
**Source:** [14-checkpoint](https://github.com/cfenollosa/os-tutorial/tree/master/14-checkpoint)

**Goal:** Reorganize code into folders, add Makefile, set up GDB debugging.

**Recommended folder structure:**
```
boot/
  boot.asm
kernel/
  kernel.c
  kernel_entry.asm
drivers/
  screen.c
  screen.h
cpu/
  idt.c
  isr.c
libc/
  mem.c
  string.c
Makefile
link.ld
```

**Debug with QEMU + GDB:**
```bash
qemu-system-i386 -fda os.bin -s -S &  # -s = :1234, -S = wait for gdb
gdb
(gdb) target remote localhost:1234
(gdb) set architecture i8086
(gdb) break kernel_main
(gdb) continue
```

---

## PHASE 4 — Drivers

---

### LESSON 15 — I/O Ports
**Source:** [15-video-ports](https://github.com/cfenollosa/os-tutorial/tree/master/15-video-ports)

**Goal:** Communicate with hardware via I/O port instructions.

**Rules:**
- Use `in`/`out` x86 instructions to read/write hardware registers
- Wrap them in C with inline assembly
- VGA cursor position is controlled via ports `0x3D4` / `0x3D5`

```c
void port_byte_out(unsigned short port, unsigned char data) {
    __asm__("out %%al, %%dx" : : "a"(data), "d"(port));
}
```

---

### LESSON 16 — VGA Screen Driver
**Source:** [16-video-driver](https://github.com/cfenollosa/os-tutorial/tree/master/16-video-driver)

**Goal:** Build a full screen driver with `kprint()`.

**Rules:**
- VGA text buffer: `0xB8000`, 80 columns × 25 rows
- Each cell = 2 bytes (char + attribute)
- Track cursor position manually
- Implement: `kprint(str)`, `kprint_at(str, col, row)`, `clear_screen()`

---

### LESSON 17 — Screen Scrolling
**Source:** [17-video-scroll](https://github.com/cfenollosa/os-tutorial/tree/master/17-video-scroll)

**Goal:** Scroll screen up when text reaches the bottom.

**Rules:**
- When cursor goes past row 25: copy rows 1-24 up to rows 0-23
- Clear the last row
- Use `memory_copy()` (your own implementation of `memcpy`)

---

## PHASE 5 — Interrupts

---

### LESSON 18 — Interrupt Descriptor Table (IDT)
**Source:** [18-interrupts](https://github.com/cfenollosa/os-tutorial/tree/master/18-interrupts)

**Goal:** Set up IDT to handle CPU exceptions and hardware interrupts.

**Rules:**
- IDT has 256 entries (descriptors)
- Each entry = 8 bytes with handler address, segment selector, flags
- Use `lidt` instruction to load IDT register
- ISRs 0–31 = CPU exceptions (divide by zero, page fault, etc.)
- ISRs 32–47 = Hardware IRQs (remapped from PIC)

**Exception ISRs to implement (minimum):**
| ISR | Exception |
|-----|-----------|
| 0 | Divide by Zero |
| 6 | Invalid Opcode |
| 8 | Double Fault |
| 13 | General Protection Fault |
| 14 | Page Fault |

---

### LESSON 19 — IRQs & PIC Remapping
**Source:** [19-interrupts-irqs](https://github.com/cfenollosa/os-tutorial/tree/master/19-interrupts-irqs)

**Goal:** Configure the 8259 PIC and handle hardware IRQs.

**Rules:**
- Remap PIC: IRQs 0–7 → INT 32–39, IRQs 8–15 → INT 40–47
- After handling an IRQ, send EOI (End of Interrupt) to PIC
- Use ports `0x20`/`0xA0` for master/slave PIC commands

```c
// End of Interrupt
port_byte_out(0x20, 0x20);  // master
// if IRQ >= 8: port_byte_out(0xA0, 0x20);  // slave too
```

---

### LESSON 20 — Timer & Keyboard IRQs
**Source:** [20-interrupts-timer](https://github.com/cfenollosa/os-tutorial/tree/master/20-interrupts-timer)

**Goal:** Handle the CPU timer (IRQ0) and keyboard input (IRQ1).

**Rules:**
- Timer fires at ~18.2 Hz by default; reprogram PIT to desired frequency
- Keyboard: read scancode from port `0x60`
- Maintain a scancode-to-ASCII lookup table
- Implement `init_timer(uint32_t frequency)` using PIT channels

```c
// Read keyboard scancode
uint8_t scancode = port_byte_in(0x60);
```

---

## PHASE 6 — Shell & Memory

---

### LESSON 21 — Basic Shell
**Source:** [21-shell](https://github.com/cfenollosa/os-tutorial/tree/master/21-shell)

**Goal:** Parse keyboard input and execute simple commands.

**Rules:**
- Buffer keyboard input until Enter is pressed
- Parse the buffer into command + arguments
- Implement at minimum: `help`, `clear`, `echo`
- Print prompt after each command completes

---

### LESSON 22 — Kernel Memory Allocator (kmalloc)
**Source:** [22-malloc](https://github.com/cfenollosa/os-tutorial/tree/master/22-malloc)

**Goal:** Implement a simple kernel heap allocator.

**Rules:**
- Start with a bump allocator (pointer that only moves forward)
- `kmalloc(size)` returns aligned memory block
- Support page-aligned allocations
- Later: implement `kfree()` with a free list

```c
uint32_t free_mem_addr = 0x10000;  // start of free memory

uint32_t kmalloc(uint32_t size) {
    uint32_t ret = free_mem_addr;
    free_mem_addr += size;
    return ret;
}
```

---

## PHASE 7 — Advanced (Linux Kernel Reference)

> From here, study the Linux kernel source: https://github.com/torvalds/linux

---

### 7.1 — Virtual Memory & Paging

**Linux reference:** `arch/x86/mm/`, `mm/`, `Documentation/mm/`

**Rules:**
- Enable paging by setting bit 31 of `CR0` (after setting `CR3` to page directory)
- Page directory: 1024 entries × 4KB pages = 4GB virtual address space
- Each page = 4096 bytes
- Implement: `init_paging()`, `map_page(virt, phys)`, `unmap_page(virt)`
- Study: `linux/arch/x86/mm/init.c`

---

### 7.2 — Process Scheduling

**Linux reference:** `kernel/sched/`, `Documentation/scheduler/`

**Rules:**
- Each process needs its own stack, register state (context), and PID
- Context switch = save current registers, load next process registers
- Start with round-robin scheduler
- Use timer IRQ to trigger scheduler
- Study: `linux/kernel/sched/core.c`

---

### 7.3 — Filesystem

**Linux reference:** `fs/`, `Documentation/filesystems/`

**Rules:**
- Implement a Virtual Filesystem (VFS) abstraction layer
- Start with a simple in-memory filesystem (ramfs/tmpfs pattern)
- Key operations: `open`, `read`, `write`, `close`, `mkdir`, `ls`
- Study: `linux/fs/ramfs/`, `linux/fs/vfs/`

---

### 7.4 — System Calls

**Linux reference:** `arch/x86/entry/syscalls/`, `kernel/sys.c`

**Rules:**
- Use `int 0x80` (Linux-style) or `syscall` instruction for user→kernel transition
- Syscall table maps syscall numbers to kernel functions
- Save all registers on entry, restore on exit
- Implement: `sys_write`, `sys_read`, `sys_exit`, `sys_fork`
- Study: `linux/arch/x86/entry/syscalls/syscall_32.tbl`

---

### 7.5 — User Mode

**Linux reference:** `arch/x86/kernel/`, `Documentation/userspace-api/`

**Rules:**
- Use CPU ring 3 (CPL=3) for user mode, ring 0 for kernel
- User processes get their own virtual address space
- Use TSS (Task State Segment) for stack switching on privilege change
- Implement `exec()` to load and run a user-space binary

---

### 7.6 — Device Drivers

**Linux reference:** `drivers/`, `Documentation/driver-api/`

**Rules:**
- Register drivers in a driver table with `init`, `read`, `write`, `ioctl` ops
- Follow the Linux pattern: character device vs block device
- Implement at minimum: keyboard driver, serial driver, disk driver
- Study: `linux/drivers/char/`, `linux/drivers/input/keyboard/`

---

### 7.7 — Networking Stack

**Linux reference:** `net/`, `Documentation/networking/`

**Rules:**
- Implement layers bottom-up: NIC driver → Ethernet → IP → TCP/UDP
- Use socket abstraction for userspace interface
- Study: `linux/net/ipv4/`, `linux/drivers/net/`

---

## MASTER BUILD ORDER CHECKLIST

```
[ ] Phase 0  — Tools installed (nasm, qemu, cross-gcc)
[ ] Phase 1  — Boot sector boots and prints text
[ ] Phase 1  — Disk read working, kernel loaded from disk
[ ] Phase 2  — GDT defined and protected mode entered
[ ] Phase 3  — C kernel compiles and runs
[ ] Phase 3  — Code organized with Makefile + linker script
[ ] Phase 4  — VGA screen driver with kprint + scrolling
[ ] Phase 4  — I/O port helpers working
[ ] Phase 5  — IDT set up with CPU exception handlers
[ ] Phase 5  — PIC remapped, timer + keyboard IRQs handled
[ ] Phase 6  — Shell reads keyboard input, parses commands
[ ] Phase 6  — kmalloc working
[ ] Phase 7  — Paging enabled
[ ] Phase 7  — Basic process scheduler
[ ] Phase 7  — System calls (int 0x80)
[ ] Phase 7  — User mode processes
[ ] Phase 7  — Simple filesystem (VFS + ramfs)
[ ] Phase 7  — Networking (optional)
```

---

## KEY REFERENCES

| Resource | URL |
|----------|-----|
| os-tutorial (cfenollosa) | https://github.com/cfenollosa/os-tutorial |
| Linux kernel source | https://github.com/torvalds/linux |
| OSDev Wiki | https://wiki.osdev.org |
| Linux kernel docs | https://www.kernel.org/doc/html/latest/ |
| x86 Manual (Intel) | https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html |
| JamesM kernel tutorial | https://web.archive.org/web/20160412174753/http://www.jamesmolloy.co.uk/tutorial_html/index.html |
| Little OS Book | https://littleosbook.github.io |

---

## GOLDEN RULES (Always Follow)

1. **Go in order.** Every lesson depends on the previous one.
2. **Test in QEMU first.** Never test on real hardware until it works in emulator.
3. **Cross-compile always.** Never use the host `gcc` for kernel code.
4. **No libc.** You are the OS — you implement malloc, memcpy, printf yourself.
5. **Understand before copying.** Read the README of each lesson before the code.
6. **Interrupts are your OS heartbeat.** Get them right before anything else.
7. **One thing at a time.** Add one feature, boot, verify, then continue.
8. **Read Linux source.** For every advanced feature, find how Linux does it — don't guess.
