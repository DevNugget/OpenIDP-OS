bits 64

global irq_stub_32

%macro isr_err_stub 1
isr_stub_%+%1:
    cli
    mov rdi, %1            ; vector
    mov rsi, [rsp]         ; error code
    mov rdx, [rsp+8]       ; RIP
    extern exception_handler
    call exception_handler
    add rsp, 8             ; pop error code
    iretq
%endmacro

%macro isr_no_err_stub 1
isr_stub_%+%1:
    cli
    push 0                 ; fake error code to unify layout
    mov rdi, %1            ; vector
    mov rsi, [rsp]         ; error code (0)
    mov rdx, [rsp+8]       ; RIP
    extern exception_handler
    call exception_handler
    add rsp, 8             ; pop fake error
    iretq
%endmacro

%macro irq_stub 1
irq_stub_%+%1:
    ; 1. CPU has already pushed SS, RSP, RFLAGS, CS, RIP
    
    ; 2. Push general purpose registers (Context)
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

    ; 3. Prepare arguments for C handler
    mov rdi, %1          ; Argument 1: IRQ Number
    mov rsi, rsp         ; Argument 2: Current Stack Pointer (points to R15)

    extern irq_handler
    cld                  ; Clear direction flag (standard ABI requirement)
    call irq_handler     ; Returns the NEW Stack Pointer (RSP) in RAX

    ; 4. Switch Stacks
    mov rsp, rax         ; Switch to the new task's stack!

    ; 5. Restore registers (from the NEW stack)
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

    ; 6. Return from interrupt
    iretq
%endmacro

section .text
    extern exception_handler
    isr_no_err_stub 0
    isr_no_err_stub 1
    isr_no_err_stub 2
    isr_no_err_stub 3
    isr_no_err_stub 4
    isr_no_err_stub 5
    isr_no_err_stub 6
    isr_no_err_stub 7
    isr_err_stub    8
    isr_no_err_stub 9
    isr_err_stub    10
    isr_err_stub    11
    isr_err_stub    12
    isr_err_stub    13
    isr_err_stub    14
    isr_no_err_stub 15
    isr_no_err_stub 16
    isr_err_stub    17
    isr_no_err_stub 18
    isr_no_err_stub 19
    isr_no_err_stub 20
    isr_no_err_stub 21
    isr_no_err_stub 22
    isr_no_err_stub 23
    isr_no_err_stub 24
    isr_no_err_stub 25
    isr_no_err_stub 26
    isr_no_err_stub 27
    isr_no_err_stub 28
    isr_no_err_stub 29
    isr_err_stub    30
    isr_no_err_stub 31

    irq_stub 32
    irq_stub 33
    irq_stub 34
    irq_stub 35
    irq_stub 36
    irq_stub 37
    irq_stub 38
    irq_stub 39
    irq_stub 40
    irq_stub 41
    irq_stub 42
    irq_stub 43
    irq_stub 44
    irq_stub 45
    irq_stub 46
    irq_stub 47

section .data
    global isr_stub_table
    isr_stub_table:
    %assign i 0 
    %rep    48  ; Changed from 32 to 48 (32 exceptions + 16 IRQs)
        %if i < 32
            dq isr_stub_%+i 
        %else
            dq irq_stub_%+i  ; Note: IRQ stubs use irq_stub_ prefix
        %endif
    %assign i i+1 
    %endrep