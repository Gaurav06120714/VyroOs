#include "shell.h"
#include "pmm.h"
#include "heap.h"
#include "../drivers/screen.h"
#include "../drivers/keyboard.h"
#include "../drivers/timer.h"
#include "../drivers/rtc.h"
#include "vfs.h"
#include "../include/types.h"

// ─────────────────────────────────────────────────
// Constants
// ─────────────────────────────────────────────────
#define MAX_INPUT      256
#define MAX_ARGS       16
#define HISTORY_SIZE   10

// Arrow key special values (set by keyboard driver)
#define KEY_UP    0x01
#define KEY_DOWN  0x02
#define KEY_LEFT  0x03
#define KEY_RIGHT 0x04

// ─────────────────────────────────────────────────
// String utilities (no stdlib in freestanding env)
// ─────────────────────────────────────────────────
static int kstrlen(const char* s) {
    int i = 0;
    while (s[i]) i++;
    return i;
}

static int kstrcmp(const char* a, const char* b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return *a - *b;
}

static void kstrcpy(char* dst, const char* src) {
    while ((*dst++ = *src++));
}

// ─────────────────────────────────────────────────
// Command history ring buffer
// ─────────────────────────────────────────────────
static char    history[HISTORY_SIZE][MAX_INPUT];
static int     history_count   = 0;

static void history_push(const char* cmd) {
    if (cmd[0] == '\0') return;
    // Don't duplicate last entry
    if (history_count > 0) {
        int last = (history_count - 1) % HISTORY_SIZE;
        if (kstrcmp(history[last], cmd) == 0) return;
    }
    kstrcpy(history[history_count % HISTORY_SIZE], cmd);
    history_count++;
}

// ─────────────────────────────────────────────────
// Argument parser — splits input into argv tokens
// "echo hello world" → argv[0]="echo" argv[1]="hello" argv[2]="world"
// ─────────────────────────────────────────────────
static char* argv[MAX_ARGS];
static int   argc;
static char  parse_buf[MAX_INPUT];

static void parse_args(const char* input) {
    argc = 0;
    kstrcpy(parse_buf, input);
    char* p = parse_buf;

    while (*p && argc < MAX_ARGS) {
        // Skip spaces
        while (*p == ' ') p++;
        if (*p == '\0') break;

        argv[argc++] = p;

        // Find end of token
        while (*p && *p != ' ') p++;
        if (*p == ' ') *p++ = '\0';
    }
}

// ─────────────────────────────────────────────────
// Built-in commands
// ─────────────────────────────────────────────────
static void cmd_help() {
    print_color("\n  Vyro OS Shell Commands\n", YELLOW_ON_BLACK);
    print_color("  ─────────────────────────────────────\n", MAKE_COLOR(COLOR_DARK_GREY, COLOR_BLACK));
    print_color("  help     ", MAKE_COLOR(COLOR_LIGHT_CYAN, COLOR_BLACK));
    print("Show this help menu\n");
    print_color("  clear    ", MAKE_COLOR(COLOR_LIGHT_CYAN, COLOR_BLACK));
    print("Clear the screen\n");
    print_color("  echo     ", MAKE_COLOR(COLOR_LIGHT_CYAN, COLOR_BLACK));
    print("Print text to screen\n");
    print_color("  version  ", MAKE_COLOR(COLOR_LIGHT_CYAN, COLOR_BLACK));
    print("Show OS version info\n");
    print_color("  about    ", MAKE_COLOR(COLOR_LIGHT_CYAN, COLOR_BLACK));
    print("About Vyro OS\n");
    print_color("  color    ", MAKE_COLOR(COLOR_LIGHT_CYAN, COLOR_BLACK));
    print("Change terminal color (0-15)\n");
    print_color("  history  ", MAKE_COLOR(COLOR_LIGHT_CYAN, COLOR_BLACK));
    print("Show command history\n");
    print_color("  mem      ", MAKE_COLOR(COLOR_LIGHT_CYAN, COLOR_BLACK));
    print("Show memory usage\n");
    print_color("  uptime   ", MAKE_COLOR(COLOR_LIGHT_CYAN, COLOR_BLACK));
    print("Show system uptime\n");
    print_color("  date     ", MAKE_COLOR(COLOR_LIGHT_CYAN, COLOR_BLACK));
    print("Show current date & time\n");
    print_color("  time     ", MAKE_COLOR(COLOR_LIGHT_CYAN, COLOR_BLACK));
    print("Show current time\n");
    print_color("  ls       ", MAKE_COLOR(COLOR_LIGHT_CYAN, COLOR_BLACK));
    print("List directory contents\n");
    print_color("  pwd      ", MAKE_COLOR(COLOR_LIGHT_CYAN, COLOR_BLACK));
    print("Print working directory\n");
    print_color("  cd       ", MAKE_COLOR(COLOR_LIGHT_CYAN, COLOR_BLACK));
    print("Change directory\n");
    print_color("  mkdir    ", MAKE_COLOR(COLOR_LIGHT_CYAN, COLOR_BLACK));
    print("Create directory\n");
    print_color("  touch    ", MAKE_COLOR(COLOR_LIGHT_CYAN, COLOR_BLACK));
    print("Create empty file\n");
    print_color("  write    ", MAKE_COLOR(COLOR_LIGHT_CYAN, COLOR_BLACK));
    print("Write text to a file\n");
    print_color("  cat      ", MAKE_COLOR(COLOR_LIGHT_CYAN, COLOR_BLACK));
    print("Show file contents\n");
    print_color("  rm       ", MAKE_COLOR(COLOR_LIGHT_CYAN, COLOR_BLACK));
    print("Remove file or directory\n");
    print_color("  tree     ", MAKE_COLOR(COLOR_LIGHT_CYAN, COLOR_BLACK));
    print("Show filesystem tree\n");
    print_color("  sleep    ", MAKE_COLOR(COLOR_LIGHT_CYAN, COLOR_BLACK));
    print("Sleep N milliseconds\n");
    print_color("  reboot   ", MAKE_COLOR(COLOR_LIGHT_CYAN, COLOR_BLACK));
    print("Reboot the system\n");
    print_color("  ─────────────────────────────────────\n\n", MAKE_COLOR(COLOR_DARK_GREY, COLOR_BLACK));
}

static void cmd_clear() {
    screen_clear(WHITE_ON_BLACK);
    // Reprint minimal header after clear
    print_color("  Vyro OS v0.6.0  |  64-bit  |  x86_64\n\n",
                MAKE_COLOR(COLOR_LIGHT_GREY, COLOR_BLACK));
}

static void cmd_echo() {
    print_char('\n');
    for (int i = 1; i < argc; i++) {
        print(argv[i]);
        if (i < argc - 1) print_char(' ');
    }
    print("\n\n");
}

static void cmd_version() {
    print_color("\n  Vyro OS\n", CYAN_ON_BLACK);
    print("  Version   : 0.6.0\n");
    print("  Phase     : 6 (Interactive Shell)\n");
    print("  Arch      : x86_64 (64-bit)\n");
    print("  Kernel    : Custom (no Linux)\n");
    print("  Language  : NASM Assembly + C\n");
    print("  License   : MIT\n");
    print("  Budget    : $0\n\n");
}

static void cmd_about() {
    print_color("\n  About Vyro OS\n", CYAN_ON_BLACK);
    print("  Built from scratch — no Linux, no BSD, no shortcuts.\n");
    print("  Every line of code written by hand.\n");
    print("  Bootloader -> Protected Mode -> Long Mode -> Kernel -> Shell\n\n");
}

static void cmd_history() {
    print_char('\n');
    if (history_count == 0) {
        print_color("  No history yet.\n\n", MAKE_COLOR(COLOR_LIGHT_GREY, COLOR_BLACK));
        return;
    }
    int start = history_count > HISTORY_SIZE ? history_count - HISTORY_SIZE : 0;
    int n = 1;
    for (int i = start; i < history_count; i++) {
        print_color("  ", WHITE_ON_BLACK);
        print_int(n++);
        print("  ");
        print(history[i % HISTORY_SIZE]);
        print_char('\n');
    }
    print_char('\n');
}

static void cmd_color() {
    if (argc < 2) {
        print_color("\n  Usage: color <0-15>\n\n", MAKE_COLOR(COLOR_LIGHT_RED, COLOR_BLACK));
        return;
    }
    // Parse number from argv[1]
    int n = 0;
    const char* p = argv[1];
    while (*p >= '0' && *p <= '9') n = n * 10 + (*p++ - '0');
    if (n < 0 || n > 15) {
        print_color("\n  Color must be 0-15\n\n", MAKE_COLOR(COLOR_LIGHT_RED, COLOR_BLACK));
        return;
    }
    screen_set_color(MAKE_COLOR(n, COLOR_BLACK));
    print("\n  Color changed.\n\n");
    screen_set_color(WHITE_ON_BLACK);
}

static void cmd_mem() {
    uint32_t total_pages = pmm_total_pages();
    uint32_t used_pages  = pmm_used_pages();
    uint32_t free_pages  = pmm_free_pages();
    uint64_t heap_u      = heap_used();
    uint64_t heap_t      = heap_total();

    print_color("\n  Physical Memory\n", YELLOW_ON_BLACK);
    print("  Total pages : "); print_int(total_pages);
    print(" ("); print_int(total_pages * 4); print(" KB)\n");
    print("  Used pages  : "); print_int(used_pages);
    print(" ("); print_int(used_pages * 4); print(" KB)\n");
    print("  Free pages  : "); print_int(free_pages);
    print(" ("); print_int(free_pages * 4); print(" KB)\n");

    print_color("\n  Heap Memory\n", YELLOW_ON_BLACK);
    print("  Total : "); print_int(heap_t / 1024); print(" KB\n");
    print("  Used  : "); print_int(heap_u); print(" bytes\n");
    print("  Free  : "); print_int((heap_t - heap_u) / 1024); print(" KB\n");

    print_color("\n  Memory Map\n", YELLOW_ON_BLACK);
    print("  0x000000 - 0x00FFFF  Bootloader + page tables\n");
    print("  0x010000 - 0x017FFF  Kernel binary\n");
    print("  0x100000 - 0x1FFFFF  PMM bitmap\n");
    print("  0x200000 - 0x4FFFFF  PMM managed pages\n");
    print("  0x500000 - 0xCFFFFF  Heap (8MB)\n\n");
}

// Print a 2-digit zero-padded number
static void print_2d(uint8_t n) {
    print_char('0' + (n / 10));
    print_char('0' + (n % 10));
}

static const char* month_names[] = {
    "", "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static void cmd_time() {
    rtc_time_t t;
    rtc_read(&t);
    print("\n  ");
    print_2d(t.hour);   print_char(':');
    print_2d(t.minute); print_char(':');
    print_2d(t.second);
    print(" (UTC)\n\n");
}

static void cmd_date() {
    rtc_time_t t;
    rtc_read(&t);
    print("\n  ");
    if (t.month >= 1 && t.month <= 12) print(month_names[t.month]);
    print_char(' ');
    print_int(t.day);
    print(", ");
    print_int(t.year);
    print("  ");
    print_2d(t.hour);   print_char(':');
    print_2d(t.minute); print_char(':');
    print_2d(t.second);
    print(" UTC\n\n");
}

static void cmd_uptime() {
    uint64_t secs  = timer_uptime_seconds();
    uint64_t ms    = timer_uptime_ms();
    uint64_t ticks = timer_ticks();

    uint64_t hours = secs / 3600;
    uint64_t mins  = (secs % 3600) / 60;
    uint64_t s     = secs % 60;

    print_color("\n  System Uptime\n", YELLOW_ON_BLACK);
    print("  ");
    print_int(hours); print("h ");
    print_int(mins);  print("m ");
    print_int(s);     print("s\n");
    print("  Total : "); print_int(ms);    print(" ms\n");
    print("  Ticks : "); print_int(ticks); print(" (@100 Hz)\n\n");
}

static void cmd_sleep() {
    if (argc < 2) {
        print_color("\n  Usage: sleep <milliseconds>\n\n", MAKE_COLOR(COLOR_LIGHT_RED, COLOR_BLACK));
        return;
    }
    uint32_t ms = 0;
    const char* p = argv[1];
    while (*p >= '0' && *p <= '9') ms = ms * 10 + (*p++ - '0');

    print("\n  Sleeping for ");
    print_int(ms);
    print(" ms...");
    sleep_ms(ms);
    print_color(" done.\n\n", MAKE_COLOR(COLOR_LIGHT_GREEN, COLOR_BLACK));
}

// ─────────────────────────────────────────────────
// Filesystem commands — track current working directory
// ─────────────────────────────────────────────────
static vfs_node_t* cwd = 0;

static void fs_ensure() {
    if (!cwd) cwd = vfs_root();
}

static void cmd_pwd() {
    fs_ensure();
    char path[256];
    vfs_full_path(cwd, path, sizeof(path));
    print("\n  ");
    print(path);
    print("\n\n");
}

static void cmd_ls() {
    fs_ensure();
    vfs_node_t* dir = cwd;

    // Optional argument: ls <dir>
    if (argc >= 2) {
        vfs_node_t* target = vfs_find(cwd, argv[1]);
        if (!target) {
            print_color("\n  ls: no such directory: ", MAKE_COLOR(COLOR_LIGHT_RED, COLOR_BLACK));
            print(argv[1]); print("\n\n");
            return;
        }
        dir = target;
    }

    print_char('\n');
    vfs_node_t* cur = dir->first_child;
    if (!cur) {
        print_color("  (empty)\n\n", MAKE_COLOR(COLOR_DARK_GREY, COLOR_BLACK));
        return;
    }
    while (cur) {
        if (cur->type == VFS_DIRECTORY) {
            print_color("  ", WHITE_ON_BLACK);
            print_color(cur->name, MAKE_COLOR(COLOR_LIGHT_BLUE, COLOR_BLACK));
            print_color("/\n", MAKE_COLOR(COLOR_LIGHT_BLUE, COLOR_BLACK));
        } else {
            print("  ");
            print(cur->name);
            print("  (");
            print_int(cur->size);
            print(" bytes)\n");
        }
        cur = cur->next_sibling;
    }
    print_char('\n');
}

static void cmd_cd() {
    fs_ensure();
    if (argc < 2) { cwd = vfs_root(); return; }

    vfs_node_t* target = vfs_find(cwd, argv[1]);
    if (!target) {
        print_color("\n  cd: no such directory: ", MAKE_COLOR(COLOR_LIGHT_RED, COLOR_BLACK));
        print(argv[1]); print("\n\n");
        return;
    }
    if (target->type != VFS_DIRECTORY) {
        print_color("\n  cd: not a directory: ", MAKE_COLOR(COLOR_LIGHT_RED, COLOR_BLACK));
        print(argv[1]); print("\n\n");
        return;
    }
    cwd = target;
}

static void cmd_mkdir() {
    fs_ensure();
    if (argc < 2) {
        print_color("\n  Usage: mkdir <name>\n\n", MAKE_COLOR(COLOR_LIGHT_RED, COLOR_BLACK));
        return;
    }
    if (!vfs_create(cwd, argv[1], VFS_DIRECTORY)) {
        print_color("\n  mkdir: cannot create '", MAKE_COLOR(COLOR_LIGHT_RED, COLOR_BLACK));
        print(argv[1]); print("' (exists?)\n\n");
    }
}

static void cmd_touch() {
    fs_ensure();
    if (argc < 2) {
        print_color("\n  Usage: touch <name>\n\n", MAKE_COLOR(COLOR_LIGHT_RED, COLOR_BLACK));
        return;
    }
    if (!vfs_create(cwd, argv[1], VFS_FILE)) {
        print_color("\n  touch: cannot create '", MAKE_COLOR(COLOR_LIGHT_RED, COLOR_BLACK));
        print(argv[1]); print("' (exists?)\n\n");
    }
}

static void cmd_write() {
    fs_ensure();
    if (argc < 3) {
        print_color("\n  Usage: write <file> <text...>\n\n", MAKE_COLOR(COLOR_LIGHT_RED, COLOR_BLACK));
        return;
    }
    vfs_node_t* file = vfs_find(cwd, argv[1]);
    if (!file) {
        // Auto-create the file
        file = vfs_create(cwd, argv[1], VFS_FILE);
        if (!file) {
            print_color("\n  write: cannot create file\n\n", MAKE_COLOR(COLOR_LIGHT_RED, COLOR_BLACK));
            return;
        }
    }
    if (file->type != VFS_FILE) {
        print_color("\n  write: not a file\n\n", MAKE_COLOR(COLOR_LIGHT_RED, COLOR_BLACK));
        return;
    }

    // Join argv[2..] into one string buffer
    static char buf[1024];
    int pos = 0;
    for (int i = 2; i < argc && pos < 1020; i++) {
        const char* w = argv[i];
        for (int j = 0; w[j] && pos < 1020; j++) buf[pos++] = w[j];
        if (i < argc - 1 && pos < 1020) buf[pos++] = ' ';
    }
    buf[pos] = '\0';

    vfs_write(file, buf);
}

static void cmd_cat() {
    fs_ensure();
    if (argc < 2) {
        print_color("\n  Usage: cat <file>\n\n", MAKE_COLOR(COLOR_LIGHT_RED, COLOR_BLACK));
        return;
    }
    vfs_node_t* file = vfs_find(cwd, argv[1]);
    if (!file) {
        print_color("\n  cat: no such file: ", MAKE_COLOR(COLOR_LIGHT_RED, COLOR_BLACK));
        print(argv[1]); print("\n\n");
        return;
    }
    if (file->type != VFS_FILE) {
        print_color("\n  cat: is a directory\n\n", MAKE_COLOR(COLOR_LIGHT_RED, COLOR_BLACK));
        return;
    }
    print("\n  ");
    print(vfs_read(file));
    print("\n\n");
}

static void cmd_rm() {
    fs_ensure();
    if (argc < 2) {
        print_color("\n  Usage: rm <name>\n\n", MAKE_COLOR(COLOR_LIGHT_RED, COLOR_BLACK));
        return;
    }
    if (!vfs_remove(cwd, argv[1])) {
        print_color("\n  rm: no such file or directory: ", MAKE_COLOR(COLOR_LIGHT_RED, COLOR_BLACK));
        print(argv[1]); print("\n\n");
    }
}

// Recursive tree printer
static void tree_print(vfs_node_t* node, int depth) {
    vfs_node_t* cur = node->first_child;
    while (cur) {
        for (int i = 0; i < depth; i++) print("  ");
        print("  ");
        if (cur->type == VFS_DIRECTORY) {
            print_color(cur->name, MAKE_COLOR(COLOR_LIGHT_BLUE, COLOR_BLACK));
            print_color("/\n", MAKE_COLOR(COLOR_LIGHT_BLUE, COLOR_BLACK));
            tree_print(cur, depth + 1);
        } else {
            print(cur->name);
            print_char('\n');
        }
        cur = cur->next_sibling;
    }
}

static void cmd_tree() {
    fs_ensure();
    print_color("\n  /\n", MAKE_COLOR(COLOR_LIGHT_BLUE, COLOR_BLACK));
    tree_print(vfs_root(), 0);
    print_char('\n');
}

static void cmd_reboot() {
    print_color("\n  Rebooting Vyro OS...\n", YELLOW_ON_BLACK);
    // Triple fault reboot — write bad IDT and trigger interrupt
    uint8_t bad[6] = {0};
    __asm__ volatile("lidt %0" : : "m"(bad));
    __asm__ volatile("int $0x00");
}

static void cmd_not_found(const char* cmd) {
    print_color("\n  vyro: command not found: ", MAKE_COLOR(COLOR_LIGHT_RED, COLOR_BLACK));
    print(cmd);
    print_color("\n  Type 'help' for available commands.\n\n",
                MAKE_COLOR(COLOR_LIGHT_GREY, COLOR_BLACK));
}

// ─────────────────────────────────────────────────
// Command dispatch table
// ─────────────────────────────────────────────────
typedef struct {
    const char* name;
    void (*handler)();
} command_t;

static const command_t commands[] = {
    { "help",    cmd_help    },
    { "clear",   cmd_clear   },
    { "cls",     cmd_clear   },
    { "echo",    cmd_echo    },
    { "version", cmd_version },
    { "ver",     cmd_version },
    { "about",   cmd_about   },
    { "history", cmd_history },
    { "color",   cmd_color   },
    { "mem",     cmd_mem     },
    { "uptime",  cmd_uptime  },
    { "date",    cmd_date    },
    { "time",    cmd_time    },
    { "sleep",   cmd_sleep   },
    { "ls",      cmd_ls      },
    { "pwd",     cmd_pwd     },
    { "cd",      cmd_cd      },
    { "mkdir",   cmd_mkdir   },
    { "touch",   cmd_touch   },
    { "write",   cmd_write   },
    { "cat",     cmd_cat     },
    { "rm",      cmd_rm      },
    { "tree",    cmd_tree    },
    { "reboot",  cmd_reboot  },
};

#define NUM_COMMANDS (sizeof(commands) / sizeof(commands[0]))

static void dispatch(const char* input) {
    if (input[0] == '\0') return;

    parse_args(input);
    if (argc == 0) return;

    for (uint32_t i = 0; i < NUM_COMMANDS; i++) {
        if (kstrcmp(argv[0], commands[i].name) == 0) {
            commands[i].handler();
            return;
        }
    }
    cmd_not_found(argv[0]);
}

// ─────────────────────────────────────────────────
// Print the shell prompt
// ─────────────────────────────────────────────────
static void print_prompt() {
    fs_ensure();
    char path[256];
    vfs_full_path(cwd, path, sizeof(path));
    print_color("vyro", MAKE_COLOR(COLOR_LIGHT_GREEN, COLOR_BLACK));
    print_color(":", MAKE_COLOR(COLOR_LIGHT_GREY, COLOR_BLACK));
    print_color(path, MAKE_COLOR(COLOR_LIGHT_BLUE, COLOR_BLACK));
    print_color("> ", MAKE_COLOR(COLOR_LIGHT_GREY, COLOR_BLACK));
}

// ─────────────────────────────────────────────────
// shell_init: print welcome message
// ─────────────────────────────────────────────────
void shell_init() {
    print_color("  Type 'help' for commands. Use UP/DOWN for history.\n\n",
                MAKE_COLOR(COLOR_LIGHT_GREY, COLOR_BLACK));
    print_prompt();
}

// ─────────────────────────────────────────────────
// shell_run: main shell loop (never returns)
// ─────────────────────────────────────────────────
void shell_run() {
    static char input[MAX_INPUT];
    static int  input_len = 0;
    static int  hist_nav  = -1;

    while (1) {
        __asm__ volatile("hlt");

        while (keyboard_has_input()) {
            char c = keyboard_getchar();

            // ── Arrow keys ──
            if (c == KEY_UP) {
                if (history_count == 0) continue;
                if (hist_nav == -1)
                    hist_nav = history_count - 1;
                else if (hist_nav > 0)
                    hist_nav--;

                // Clear current line
                for (int i = 0; i < input_len; i++) {
                    print_char('\b');
                }
                kstrcpy(input, history[hist_nav % HISTORY_SIZE]);
                input_len = kstrlen(input);
                print(input);
                continue;
            }

            if (c == KEY_DOWN) {
                if (hist_nav == -1) continue;
                hist_nav++;
                for (int i = 0; i < input_len; i++) {
                    print_char('\b');
                }
                if (hist_nav >= history_count) {
                    hist_nav = -1;
                    input[0] = '\0';
                    input_len = 0;
                } else {
                    kstrcpy(input, history[hist_nav % HISTORY_SIZE]);
                    input_len = kstrlen(input);
                    print(input);
                }
                continue;
            }

            // ── Backspace ──
            if (c == KEY_BACKSPACE) {
                if (input_len > 0) {
                    input_len--;
                    print_char('\b');
                    print_char(' ');
                    print_char('\b');
                }
                continue;
            }

            // ── Enter ──
            if (c == '\n') {
                print_char('\n');
                input[input_len] = '\0';
                history_push(input);
                hist_nav  = -1;
                dispatch(input);
                input_len = 0;
                input[0]  = '\0';
                print_prompt();
                continue;
            }

            // ── Printable character ──
            if (c >= ' ' && c < 127 && input_len < MAX_INPUT - 1) {
                input[input_len++] = c;
                print_char(c);
            }
        }
    }
}
