[bits 64]

; Saved kernel stack pointer so SYS_EXIT can unwind back here
[global saved_kernel_rsp]
section .bss
saved_kernel_rsp: resq 1

section .text

; ─────────────────────────────────────────────────
; enter_user_mode(uint64_t entry, uint64_t user_stack)
;   rdi = entry point (ring-3 code)
;   rsi = top of user stack
;
; Crafts an iretq frame and drops to ring 3.
; Returns here (to the C caller) when ring-3 code
; invokes SYS_EXIT, via return_to_kernel below.
; ─────────────────────────────────────────────────
[global enter_user_mode]
enter_user_mode:
    ; Save the kernel stack so we can come back later
    mov [rel saved_kernel_rsp], rsp

    ; User data selector (0x20 | 3 = 0x23) into segment registers
    mov ax, 0x23
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Build the iretq frame (CPU pops these in order):
    ;   SS, RSP, RFLAGS, CS, RIP
    push 0x23               ; SS  = user data | 3
    push rsi                ; RSP = user stack top
    pushfq                  ; RFLAGS
    pop rax
    or rax, 0x200           ; set IF (enable interrupts in ring 3)
    push rax
    push 0x1B               ; CS  = user code | 3
    push rdi                ; RIP = entry point
    iretq

; ─────────────────────────────────────────────────
; return_to_kernel() — called from the syscall handler
; when ring-3 code invokes SYS_EXIT. Restores the kernel
; stack and returns into enter_user_mode's C caller.
; ─────────────────────────────────────────────────
[global return_to_kernel]
return_to_kernel:
    ; Restore kernel data segments
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Switch back to the saved kernel stack and return to caller
    mov rsp, [rel saved_kernel_rsp]
    ret
