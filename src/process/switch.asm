bits 64
section .text

global switch_context
global get_current_rsp

; void switch_context(process_t *old, process_t *new)
switch_context:
    ; Save old context
    pushfq
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
    
    ; Save old RSP into old->rsp
    mov [rdi], rsp
    
    ; Load new RSP from new->rsp
    mov rsp, [rsi]
    
    ; Restore new context
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
    popfq
    
    ret

; uint64_t get_current_rsp(void)
get_current_rsp:
    mov rax, rsp
    ret