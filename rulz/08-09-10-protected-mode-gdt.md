# LESSON 08-09-10 — Protected Mode & GDT
> Sources:
> - https://github.com/cfenollosa/os-tutorial/tree/master/08-32bit-print
> - https://github.com/cfenollosa/os-tutorial/tree/master/09-32bit-gdt
> - https://github.com/cfenollosa/os-tutorial/tree/master/10-32bit-enter

## Concepts
32-bit protected mode, VGA memory, GDT, segment descriptors, CR0, far jump

---

## LESSON 08 — Print in 32-bit Mode (VGA Direct)

### Goal
Write text directly to VGA memory — no BIOS in protected mode.

### Theory
- VGA text buffer starts at physical address `0xB8000`
- 80 columns × 25 rows = 2000 characters
- Each character = **2 bytes**: `[ASCII][Attribute]`
- Attribute byte: high nibble = background color, low nibble = foreground color
  - `0x07` = white on black
  - `0x0F` = bright white on black

### Formula
```
offset = 2 × (row × 80 + col)
address = 0xB8000 + offset
```

### Code
```nasm
[bits 32]

VIDEO_MEMORY equ 0xB8000
WHITE_ON_BLACK equ 0x0f

print_string_pm:
    pusha
    mov edx, VIDEO_MEMORY
.loop:
    mov al, [ebx]        ; next character
    mov ah, WHITE_ON_BLACK
    cmp al, 0
    je  .done
    mov [edx], ax        ; write char + attribute
    inc ebx
    add edx, 2           ; next VGA cell (2 bytes per char)
    jmp .loop
.done:
    popa
    ret
```

---

## LESSON 09 — Global Descriptor Table (GDT)

### Goal
Define memory segments the CPU needs before entering protected mode.

### Theory
In 32-bit protected mode, the segment registers (`CS`, `DS`, etc.) don't hold addresses —
they hold **indices** into the GDT. Each GDT entry (8 bytes) describes:
- Base address (32 bits)
- Size/limit (20 bits)
- Flags (permissions, type, granularity)

### GDT Structure
```
GDT Entry Layout (8 bytes):
Bits 0-15:  limit bits 0-15
Bits 16-31: base bits 0-15
Bits 32-39: base bits 16-23
Bits 40-47: access byte (present, privilege, type flags)
Bits 48-51: limit bits 16-19
Bits 52-55: flags (granularity, 32-bit, etc.)
Bits 56-63: base bits 24-31
```

### Code

**File: `boot/32bit-gdt.asm`**
```nasm
gdt_start:
    ; Null descriptor (required — first entry always null)
    dd 0x0
    dd 0x0

gdt_code:               ; Code segment: base=0, limit=0xFFFFF
    dw 0xffff           ; limit bits 0-15
    dw 0x0              ; base bits 0-15
    db 0x0              ; base bits 16-23
    db 10011010b        ; access byte: present, ring 0, code, executable, readable
    db 11001111b        ; flags: 4KB granularity, 32-bit + limit bits 16-19
    db 0x0              ; base bits 24-31

gdt_data:               ; Data segment: same base/limit
    dw 0xffff
    dw 0x0
    db 0x0
    db 10010010b        ; access byte: present, ring 0, data, readable/writable
    db 11001111b
    db 0x0

gdt_end:

; GDT descriptor — loaded with lgdt
gdt_descriptor:
    dw gdt_end - gdt_start - 1   ; size (always 1 less than true size)
    dd gdt_start                 ; address

; Segment selector constants (offset from gdt_start)
CODE_SEG equ gdt_code - gdt_start   ; = 0x08
DATA_SEG equ gdt_data - gdt_start   ; = 0x10
```

---

## LESSON 10 — Enter 32-bit Protected Mode

### Goal
Actually switch the CPU from real mode to 32-bit protected mode.

### The Switch (STRICT ORDER)
```nasm
[bits 16]
switch_to_pm:
    cli                         ; 1. Disable ALL interrupts

    lgdt [gdt_descriptor]       ; 2. Load GDT register

    mov eax, cr0                ; 3. Set bit 0 of CR0 (protection enable)
    or  eax, 0x1
    mov cr0, eax

    jmp CODE_SEG:init_pm        ; 4. Far jump — flushes CPU pipeline, loads CS

[bits 32]
init_pm:
    mov ax, DATA_SEG            ; 5. Update all segment registers
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov ebp, 0x90000            ; 6. Set up new stack (32-bit)
    mov esp, ebp

    call BEGIN_PM               ; 7. Jump to your kernel/32-bit code
```

## Build Steps
```bash
nasm -f bin 32bit-main.asm -o 32bit.bin
qemu-system-i386 -fda 32bit.bin
```

## Expected Result
Two messages on screen from 32-bit code — one from 16-bit before switch, one from 32-bit after.

## Common Problems & Solutions

| Problem | Solution |
|---------|----------|
| Triple fault / instant reboot | GDT is wrong — double check the descriptor bytes; also check far jump uses `CODE_SEG` |
| Screen corruption in 32-bit print | Wrong VGA address or wrong attribute byte — use `0xB8000` and `0x07` |
| `lgdt` fails / hangs | GDT descriptor size must be `gdt_end - gdt_start - 1` (one less) |
| Protected mode code uses wrong segment | After mode switch, update ALL segment registers (`ds`, `es`, `fs`, `gs`, `ss`) to `DATA_SEG` |
| BIOS interrupts still called in 32-bit | They DON'T work in protected mode — write your own drivers instead |
| Jump after `mov cr0` doesn't flush pipeline | Must be a FAR jump (with segment): `jmp CODE_SEG:label`, not `jmp label` |

## Rules to Remember
- `cli` before EVERYTHING — no interrupts during mode switch
- GDT null entry at index 0 is mandatory
- Far jump after `CR0` bit set — forces CPU to reload instruction pipeline
- After switch: update ALL segment registers to `DATA_SEG`
- `CODE_SEG = 0x08`, `DATA_SEG = 0x10` (offsets into GDT)
- No BIOS interrupts (`int 0x10`, `int 0x13`) ever again after this point
