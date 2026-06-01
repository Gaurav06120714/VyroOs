ENTRY(_start)

SECTIONS {
    /* Kernel loads at 0x10000 in physical memory */
    . = 0x10000;

    .text : {
        *(.text)        /* All code sections */
    }

    .rodata : {
        *(.rodata)      /* Read-only data (string literals, constants) */
    }

    .data : {
        *(.data)        /* Initialized global/static variables */
    }

    .bss : {
        *(COMMON)
        *(.bss)         /* Uninitialized globals — zeroed at boot */
    }
}
