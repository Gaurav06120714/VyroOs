[bits 64]

; ─────────────────────────────────────────────────
; isr_common_stub: shared handler called by all ISRs
; Saves all registers, calls C isr_handler, restores
; ─────────────────────────────────────────────────
[extern isr_handler]
[extern irq_handler]

isr_common_stub:
    ; Save all general-purpose registers
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; Pass pointer to stack frame (registers_t*) as first argument
    mov rdi, rsp
    call isr_handler

    ; Restore all registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; Remove int_no and error_code from stack
    add rsp, 16

    iretq

irq_common_stub:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, rsp
    call irq_handler

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    add rsp, 16
    iretq

; ─────────────────────────────────────────────────
; Macro: ISR without error code
; Pushes dummy 0 so stack layout is uniform
; ─────────────────────────────────────────────────
%macro ISR_NOERR 1
[global isr%1]
isr%1:
    push qword 0        ; Dummy error code
    push qword %1       ; Interrupt number
    jmp isr_common_stub
%endmacro

; ─────────────────────────────────────────────────
; Macro: ISR with error code
; CPU already pushed error code before calling
; ─────────────────────────────────────────────────
%macro ISR_ERR 1
[global isr%1]
isr%1:
    push qword %1       ; Interrupt number (error code already on stack)
    jmp isr_common_stub
%endmacro

; ─────────────────────────────────────────────────
; Macro: IRQ handlers
; All map to irq_common_stub
; ─────────────────────────────────────────────────
%macro IRQ 2
[global irq%1]
irq%1:
    push qword 0        ; No error code
    push qword %2       ; Interrupt vector number
    jmp irq_common_stub
%endmacro

; ─────────────────────────────────────────────────
; CPU Exception ISRs (vectors 0-31)
; ─────────────────────────────────────────────────
ISR_NOERR 0   ; Divide by Zero
ISR_NOERR 1   ; Debug
ISR_NOERR 2   ; Non-Maskable Interrupt
ISR_NOERR 3   ; Breakpoint
ISR_NOERR 4   ; Overflow
ISR_NOERR 5   ; Bound Range Exceeded
ISR_NOERR 6   ; Invalid Opcode
ISR_NOERR 7   ; Device Not Available
ISR_ERR   8   ; Double Fault (has error code)
ISR_NOERR 9   ; Coprocessor Segment Overrun
ISR_ERR   10  ; Invalid TSS
ISR_ERR   11  ; Segment Not Present
ISR_ERR   12  ; Stack-Segment Fault
ISR_ERR   13  ; General Protection Fault
ISR_ERR   14  ; Page Fault
ISR_NOERR 15  ; Reserved
ISR_NOERR 16  ; x87 Floating Point Exception
ISR_ERR   17  ; Alignment Check
ISR_NOERR 18  ; Machine Check
ISR_NOERR 19  ; SIMD Floating Point Exception
ISR_NOERR 20  ; Virtualization Exception
ISR_NOERR 21  ; Reserved
ISR_NOERR 22  ; Reserved
ISR_NOERR 23  ; Reserved
ISR_NOERR 24  ; Reserved
ISR_NOERR 25  ; Reserved
ISR_NOERR 26  ; Reserved
ISR_NOERR 27  ; Reserved
ISR_NOERR 28  ; Reserved
ISR_NOERR 29  ; Reserved
ISR_ERR   30  ; Security Exception
ISR_NOERR 31  ; Reserved

; ─────────────────────────────────────────────────
; Hardware IRQ stubs (vectors 32-47)
; IRQ number, vector number
; ─────────────────────────────────────────────────
IRQ  0, 32    ; Timer
IRQ  1, 33    ; Keyboard
IRQ  2, 34    ; Cascade (slave PIC)
IRQ  3, 35    ; COM2
IRQ  4, 36    ; COM1
IRQ  5, 37    ; LPT2
IRQ  6, 38    ; Floppy
IRQ  7, 39    ; LPT1
IRQ  8, 40    ; RTC
IRQ  9, 41    ; Free
IRQ 10, 42    ; Free
IRQ 11, 43    ; Free
IRQ 12, 44    ; PS/2 Mouse
IRQ 13, 45    ; FPU
IRQ 14, 46    ; ATA Primary
IRQ 15, 47    ; ATA Secondary
