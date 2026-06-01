ENTRY(_start)

SECTIONS {
    /* User programs load at 4MB — inside the identity-mapped,
       user-accessible region of the address space. */
    . = 0x400000;

    .text : { *(.text*) }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    .bss : { *(.bss*) *(COMMON) }
}
