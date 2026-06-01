[org 0x7c00]        ; BIOS loads bootloader at this address
[bits 16]           ; Start in 16-bit real mode

start:
    ; Set up segments
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00

    ; Print "VYRO OS" using BIOS interrupt
    mov si, msg
    call print

    jmp $           ; Halt — loop forever

; -------------------------
; print: prints a null-terminated string
; Input: SI = pointer to string
; -------------------------
print:
    mov ah, 0x0e    ; BIOS teletype mode
.loop:
    lodsb           ; Load next char into AL
    cmp al, 0       ; Check for null terminator
    je .done
    int 0x10        ; Print character
    jmp .loop
.done:
    ret

msg db 'VYRO OS', 0x0d, 0x0a, 0

; Pad to 510 bytes and add boot signature
times 510-($-$$) db 0
dw 0xaa55
