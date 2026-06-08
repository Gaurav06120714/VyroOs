# ADVANCED — Linux Kernel Study Guide
> Source: https://github.com/torvalds/linux
> Linux Kernel Docs: https://www.kernel.org/doc/html/latest/

This section covers what to study in the Linux kernel source for each advanced feature.
For each feature: what to build, where to study in Linux, key concepts, and common problems.

---

## PHASE A — Virtual Memory & Paging

### What to Build
- Enable x86 paging (page directory + page tables)
- Map physical memory to virtual addresses
- Handle page fault exceptions

### Linux Kernel Files to Study
```
arch/x86/mm/init.c          — x86 memory initialization
arch/x86/mm/pgtable.c       — page table operations
mm/memory.c                 — core memory management
mm/mmap.c                   — virtual memory areas
Documentation/mm/           — MM subsystem docs
```

### Key Concepts

**Page Directory + Page Tables (x86 32-bit, non-PAE):**
```
Virtual Address (32 bits):
  [31-22] = Page Directory index (10 bits) → selects PDE
  [21-12] = Page Table index (10 bits)     → selects PTE
  [11-0]  = Page Offset (12 bits)          → offset within 4KB page

Physical Address = PTE[20 bits] << 12 | offset
```

**Enable Paging:**
```c
// 1. Allocate page directory (4KB aligned, 4096 bytes)
// 2. Set page directory entry for each mapped region
// 3. Load CR3 with physical address of page directory
// 4. Set bit 31 of CR0

void init_paging() {
    // Set up identity mapping for first 4MB (kernel space)
    for (int i = 0; i < 1024; i++) {
        // Page table entry: address | present | writable
        page_table[i] = (i * 0x1000) | 0x3;
    }

    // Page directory entry: points to page table
    page_directory[0] = ((uint32_t)page_table) | 0x3;

    // Load CR3 and enable paging
    asm volatile("mov %0, %%cr3" : : "r"(page_directory));
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;  // set paging bit
    asm volatile("mov %0, %%cr0" : : "r"(cr0));
}
```

**Page Fault Handler (ISR 14):**
```c
void page_fault_handler(registers_t *r) {
    uint32_t faulting_address;
    asm volatile("mov %%cr2, %0" : "=r"(faulting_address));

    int present  = !(r->err_code & 0x1);   // page not present
    int rw       = r->err_code & 0x2;       // write operation
    int us       = r->err_code & 0x4;       // user mode
    int reserved = r->err_code & 0x8;       // reserved bits overwritten

    kprint("Page fault at 0x"); kprint_hex(faulting_address);
    if (present)  kprint(" [not present]");
    if (rw)       kprint(" [write]");
    if (us)       kprint(" [user-mode]");
    if (reserved) kprint(" [reserved]");
    kprint("\n");
    for(;;);
}
```

### Common Problems
| Problem | Solution |
|---------|----------|
| Triple fault when enabling paging | Page directory not 4KB aligned; or identity map missing for current code |
| Kernel crashes after `mov cr0` | Make sure kernel code is identity-mapped before enabling paging |
| CR2 shows wrong address | CR2 holds the faulting address — read it immediately at start of page fault handler |

---

## PHASE B — Process Scheduling

### What to Build
- Process structure (PCB — Process Control Block)
- Context switch (save/restore registers)
- Round-robin scheduler triggered by timer IRQ

### Linux Kernel Files to Study
```
kernel/sched/core.c         — scheduler core (schedule(), __schedule())
kernel/sched/fair.c         — CFS (Completely Fair Scheduler)
arch/x86/kernel/process.c   — x86 context switch
include/linux/sched.h       — task_struct definition
Documentation/scheduler/    — scheduler docs
```

### Key Concepts

**Process Control Block:**
```c
typedef struct {
    uint32_t id;           // PID
    uint32_t esp;          // saved stack pointer
    uint32_t ebp;          // saved base pointer
    uint32_t eip;          // saved instruction pointer
    uint32_t *page_dir;    // virtual address space
    uint8_t  state;        // RUNNING, READY, BLOCKED, ZOMBIE
    char     name[32];
} process_t;

#define PROCESS_MAX     64
#define STACK_SIZE      0x2000   // 8KB per process
```

**Context Switch (Assembly):**
```nasm
; void switch_context(process_t *old, process_t *new)
switch_context:
    ; Save old process state
    mov eax, [esp+4]    ; old process pointer
    mov [eax+8], esp    ; save esp
    mov [eax+4], ebp    ; save ebp
    ; eip saved by call instruction on stack

    ; Load new process state
    mov eax, [esp+8]    ; new process pointer
    mov esp, [eax+8]
    mov ebp, [eax+4]
    ; Load CR3 if different page directory
    ret
```

**Scheduler triggered by timer:**
```c
static void timer_callback(registers_t *regs) {
    tick++;
    schedule();   // switch process every tick
}

void schedule() {
    if (process_count == 0) return;

    process_t *old = current_process;
    current_process = ready_queue[tick % process_count];

    if (old != current_process)
        switch_context(old, current_process);
}
```

---

## PHASE C — System Calls

### What to Build
- `int 0x80` syscall interface (Linux-style)
- Syscall table mapping numbers to kernel functions
- Privilege level switch (ring 3 → ring 0)

### Linux Kernel Files to Study
```
arch/x86/entry/syscalls/syscall_32.tbl  — syscall numbers table
arch/x86/kernel/syscall_32.c            — syscall table
kernel/sys.c                            — generic syscall implementations
arch/x86/entry/entry_32.S              — low-level syscall entry
include/linux/syscalls.h                — syscall declarations
```

### Key Concepts

**Syscall via int 0x80:**
```c
// In user space (ring 3):
int write(int fd, const void *buf, size_t count) {
    int ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(4),    // syscall number: write = 4
          "b"(fd),   // arg1
          "c"(buf),  // arg2
          "d"(count) // arg3
    );
    return ret;
}
```

**Syscall Table:**
```c
// kernel/syscall.c
typedef int (*syscall_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

syscall_t syscall_table[] = {
    /* 0 */ sys_restart_syscall,
    /* 1 */ sys_exit,
    /* 2 */ sys_fork,
    /* 3 */ sys_read,
    /* 4 */ sys_write,
    /* 5 */ sys_open,
    /* 6 */ sys_close,
    /* 11*/ sys_execve,
    /* 20*/ sys_getpid,
};

// int 0x80 handler (ISR 128)
void syscall_handler(registers_t *r) {
    if (r->eax >= NUM_SYSCALLS) return;
    syscall_t handler = syscall_table[r->eax];
    r->eax = handler(r->ebx, r->ecx, r->edx, r->esi, r->edi);
}
```

---

## PHASE D — Virtual Filesystem (VFS)

### What to Build
- VFS abstraction layer (open, read, write, close)
- Simple in-memory filesystem (like tmpfs/ramfs)
- File descriptor table per process

### Linux Kernel Files to Study
```
fs/vfs/           — VFS implementation (if exists)
fs/ramfs/         — simplest in-memory filesystem
fs/namei.c        — name resolution (path lookup)
fs/file.c         — file descriptor operations
include/linux/fs.h — inode, file, dentry structures
Documentation/filesystems/vfs.rst — VFS overview
```

### Key Concepts

**VFS Structures:**
```c
typedef struct {
    char name[64];
    uint8_t  type;          // FILE or DIRECTORY
    uint32_t size;
    uint8_t *data;          // file contents
    // For directories: pointer to children
} vfs_node_t;

typedef struct {
    int (*open) (vfs_node_t *node, uint32_t flags);
    int (*close)(vfs_node_t *node);
    int (*read) (vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buf);
    int (*write)(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buf);
    vfs_node_t *(*readdir)(vfs_node_t *node, uint32_t index);
    vfs_node_t *(*finddir)(vfs_node_t *node, char *name);
} vfs_ops_t;
```

---

## PHASE E — User Mode

### What to Build
- Ring 3 execution (user processes)
- TSS (Task State Segment) for kernel stack on interrupt
- `exec()` to load and run user programs

### Linux Kernel Files to Study
```
arch/x86/kernel/tss.c        — TSS setup
arch/x86/kernel/process.c    — process management
arch/x86/mm/mmap.c           — user space memory mapping
fs/exec.c                    — execve implementation
Documentation/userspace-api/ — userspace interface docs
```

### Key Concepts

**Jump to Ring 3:**
```nasm
; Switch to ring 3 using iret
jump_to_usermode:
    cli
    mov ax, 0x23    ; user data segment (RPL = 3)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push 0x23       ; ss  (user stack segment)
    push USER_STACK ; esp (user stack pointer)
    pushf           ; eflags
    or dword [esp], 0x200  ; set IF flag (enable interrupts in user mode)
    push 0x1B       ; cs  (user code segment, RPL = 3)
    push USER_CODE  ; eip (entry point)
    iret            ; pops all 5 values and switches to ring 3
```

---

## PHASE F — Device Drivers

### Linux Kernel Files to Study
```
drivers/char/          — character devices (tty, random, etc.)
drivers/input/         — input devices (keyboard, mouse)
drivers/block/         — block devices (disk)
drivers/net/           — network adapters
Documentation/driver-api/  — driver writing guide
Documentation/driver-api/driver-model/  — driver model
```

### Driver Model Pattern
```c
typedef struct {
    char name[32];
    int  (*init)  (void);
    int  (*read)  (uint8_t *buf, uint32_t count);
    int  (*write) (const uint8_t *buf, uint32_t count);
    int  (*ioctl) (uint32_t cmd, void *arg);
} driver_t;

// Register a driver
void driver_register(driver_t *drv) {
    drv->init();
    // add to driver table
}
```

---

## HOW TO READ LINUX KERNEL SOURCE

### Read in this order for each subsystem:
1. `Documentation/<subsystem>/` — read the docs first
2. `include/linux/<subsystem>.h` — understand data structures
3. `<subsystem>/Kconfig` — understand compile-time options
4. Core `.c` file — start with `init_` functions
5. `arch/x86/` version — see the x86-specific implementation

### Essential Linux Kernel Commands
```bash
# Clone Linux kernel (shallow — just latest)
git clone --depth=1 https://github.com/torvalds/linux.git

# Find where a symbol is defined
grep -r "schedule()" kernel/sched/ --include="*.c"

# Find all implementations of a function
cscope -R          # build cscope database
# then: cs find f schedule

# Read a subsystem's entry point
grep -r "subsys_initcall\|core_initcall" mm/ --include="*.c"
```

### Linux Coding Style (must follow if contributing)
```bash
# Check your code
./scripts/checkpatch.pl --file my_driver.c

# Key rules:
# - Tabs (not spaces) for indentation
# - 80 character line limit
# - Function names: lowercase_with_underscores
# - No C++ style comments (//)  — use /* */
# - Error paths go to 'goto err_label'
```

---

## MASTER BUILD ORDER (All Phases)

```
[ ] Lessons 01-07   Boot sector → prints text → loads from disk
[ ] Lessons 08-10   Enter 32-bit protected mode
[ ] Lessons 11-13   C kernel compiles and runs
[ ] Lesson  14      Code organized, GDB works
[ ] Lessons 15-17   VGA driver with kprint + scroll
[ ] Lessons 18-20   IDT + PIC + timer + keyboard
[ ] Lessons 21-22   Shell + kmalloc
[ ] Lesson  23      All bug fixes applied
[ ] Phase A         Paging enabled, page fault handler
[ ] Phase B         Multiple processes + round-robin scheduler
[ ] Phase C         System calls via int 0x80
[ ] Phase D         VFS + simple in-memory filesystem
[ ] Phase E         User mode (ring 3) processes
[ ] Phase F         Device driver framework
```
