[bits 64]

; ─────────────────────────────────────────────────
; context_switch(uint64_t* old_rsp, uint64_t new_rsp)
;   rdi = pointer to where we save the current RSP
;   rsi = the RSP to switch to
;
; Saves callee-saved registers (System V ABI), swaps stacks,
; restores the new task's registers, and returns into it.
; ─────────────────────────────────────────────────
[global context_switch]
context_switch:
    ; Save callee-saved registers onto current stack
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15

    ; Save current stack pointer into *old_rsp
    mov [rdi], rsp

    ; Load the new task's stack pointer
    mov rsp, rsi

    ; Restore callee-saved registers from new stack
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx

    ; Return — pops the entry point / saved RIP off the new stack
    ret
