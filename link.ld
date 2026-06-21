ENTRY(_start)

SECTIONS {
    . = 0x10000;

    .text   : { *(.text)   }
    .rodata : { *(.rodata) }
    .data   : { *(.data)   }

    /* BSS is placed at 1MB, above the legacy ROM/VRAM hole (0xA0000-0xFFFFF).
     * Keeping it below 1MB caused variables to land on read-only video BIOS
     * ROM, where writes silently failed and reads returned ROM bytes.
     * NOLOAD keeps it out of the flat binary so the file stays small. */
    . = 0x100000;
    .bss (NOLOAD) : {
        __bss_start = .;
        *(COMMON) *(.bss)
        __bss_end = .;
    }
}
