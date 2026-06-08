# LESSON 15-16-17 — I/O Ports & VGA Screen Driver
> Sources:
> - https://github.com/cfenollosa/os-tutorial/tree/master/15-video-ports
> - https://github.com/cfenollosa/os-tutorial/tree/master/16-video-driver
> - https://github.com/cfenollosa/os-tutorial/tree/master/17-video-scroll

## Concepts
I/O ports, inline assembly, VGA text mode, cursor, kprint, screen scroll

---

## LESSON 15 — I/O Ports

### Goal
Talk to hardware devices using `in`/`out` x86 instructions from C.

### Port Helper Functions

**File: `cpu/ports.h`**
```c
unsigned char port_byte_in(unsigned short port);
void port_byte_out(unsigned short port, unsigned char data);
unsigned short port_word_in(unsigned short port);
void port_word_out(unsigned short port, unsigned short data);
```

**File: `cpu/ports.c`**
```c
unsigned char port_byte_in(unsigned short port) {
    unsigned char result;
    __asm__("in %%dx, %%al" : "=a"(result) : "d"(port));
    return result;
}

void port_byte_out(unsigned short port, unsigned char data) {
    __asm__("out %%al, %%dx" : : "a"(data), "d"(port));
}

unsigned short port_word_in(unsigned short port) {
    unsigned short result;
    __asm__("in %%dx, %%ax" : "=a"(result) : "d"(port));
    return result;
}

void port_word_out(unsigned short port, unsigned short data) {
    __asm__("out %%ax, %%dx" : : "a"(data), "d"(port));
}
```

### Read Cursor Position via VGA Ports
```c
// VGA controller ports
#define REG_SCREEN_CTRL 0x3D4
#define REG_SCREEN_DATA 0x3D5

int get_cursor_offset() {
    port_byte_out(REG_SCREEN_CTRL, 14);          // request high byte
    int offset = port_byte_in(REG_SCREEN_DATA) << 8;
    port_byte_out(REG_SCREEN_CTRL, 15);          // request low byte
    offset += port_byte_in(REG_SCREEN_DATA);
    return offset * 2;  // cells are 2 bytes each
}

void set_cursor_offset(int offset) {
    offset /= 2;
    port_byte_out(REG_SCREEN_CTRL, 14);
    port_byte_out(REG_SCREEN_DATA, (unsigned char)(offset >> 8));
    port_byte_out(REG_SCREEN_CTRL, 15);
    port_byte_out(REG_SCREEN_DATA, (unsigned char)(offset & 0xFF));
}
```

---

## LESSON 16 — VGA Screen Driver

### Goal
Build a full screen driver with `kprint()`, cursor tracking, and newline handling.

### Constants
```c
#define VIDEO_ADDRESS   0xb8000
#define MAX_ROWS        25
#define MAX_COLS        80
#define WHITE_ON_BLACK  0x0f
#define RED_ON_WHITE    0xf4

// Cell offset calculation
#define GET_SCREEN_OFFSET(col, row) (2 * ((row) * MAX_COLS + (col)))
```

### Core Function: print_char
```c
int print_char(char c, int col, int row, char attr) {
    unsigned char *vidmem = (unsigned char*) VIDEO_ADDRESS;
    if (!attr) attr = WHITE_ON_BLACK;

    // Use current cursor position if row/col are -1
    int offset;
    if (col >= 0 && row >= 0)
        offset = GET_SCREEN_OFFSET(col, row);
    else
        offset = get_cursor_offset();

    if (c == '\n') {
        // Move to start of next row
        row = offset / (2 * MAX_COLS);
        offset = GET_SCREEN_OFFSET(0, row + 1);
    } else {
        vidmem[offset] = c;
        vidmem[offset + 1] = attr;
        offset += 2;
    }

    // Scroll if needed (Lesson 17)
    offset = handle_scrolling(offset);

    set_cursor_offset(offset);
    return offset;
}
```

### Public API
```c
// Print string at current cursor position
void kprint(char *message);

// Print string at specific row/col (-1 = use current cursor)
void kprint_at(char *message, int col, int row);

// Clear the entire screen
void clear_screen();
```

### kprint_at Implementation
```c
void kprint_at(char *message, int col, int row) {
    int offset;
    if (col >= 0 && row >= 0)
        offset = GET_SCREEN_OFFSET(col, row);
    else {
        offset = get_cursor_offset();
        row    = offset / (2 * MAX_COLS);
        col    = (offset - (row * 2 * MAX_COLS)) / 2;
    }

    int i = 0;
    while (message[i] != 0) {
        offset = print_char(message[i++], col, row, WHITE_ON_BLACK);
        row = offset / (2 * MAX_COLS);
        col = (offset - (row * 2 * MAX_COLS)) / 2;
    }
}

void kprint(char *message) {
    kprint_at(message, -1, -1);
}
```

---

## LESSON 17 — Screen Scrolling

### Goal
Scroll the screen up when text reaches the bottom row.

### memory_copy helper
```c
// libc/mem.c
void memory_copy(unsigned char *source, unsigned char *dest, int nbytes) {
    for (int i = 0; i < nbytes; i++)
        *(dest + i) = *(source + i);
}
```

### handle_scrolling
```c
int handle_scrolling(int offset) {
    if (offset < MAX_ROWS * MAX_COLS * 2)
        return offset;

    // Scroll: copy rows 1-24 up to rows 0-23
    for (int i = 1; i < MAX_ROWS; i++) {
        memory_copy(
            (unsigned char*)(VIDEO_ADDRESS + GET_SCREEN_OFFSET(0, i)),
            (unsigned char*)(VIDEO_ADDRESS + GET_SCREEN_OFFSET(0, i-1)),
            MAX_COLS * 2
        );
    }

    // Blank last row
    unsigned char *last_line = (unsigned char*)(VIDEO_ADDRESS + GET_SCREEN_OFFSET(0, MAX_ROWS-1));
    for (int i = 0; i < MAX_COLS * 2; i++)
        last_line[i] = 0;

    // Move offset back by one row
    offset -= 2 * MAX_COLS;
    return offset;
}
```

## Common Problems & Solutions

| Problem | Solution |
|---------|----------|
| Cursor is off after printing | `set_cursor_offset` uses cell units (divide byte offset by 2) |
| Newline doesn't go to column 0 | Newline handling: set offset to start of next row, not just `+= 2` |
| Screen scrolls wrong | `memory_copy` source and dest are correct direction (row i → row i-1) |
| `kprint` doesn't show on screen | Check VGA address `0xB8000`, check you're in 32-bit protected mode |
| Characters show wrong colors | Attribute byte wrong — `0x07` = light grey on black, `0x0F` = bright white |
| Port read returns 0xFF | VGA ports `0x3D4`/`0x3D5` — verify you write control port before reading data port |

## Rules to Remember
- VGA cell = 2 bytes: `[char][attribute]`
- `0xB8000` = VGA text buffer base address
- Cursor offset from hardware = cell units; multiply by 2 for byte offset
- `kprint` with `-1, -1` means "at current cursor position"
- Scrolling = copy rows 1–24 → rows 0–23, then blank row 24
