.global gdt_flush
gdt_flush:
    lgdt (%rdi)
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss
    pushq $0x08
    lea .flush(%rip), %rax
    pushq %rax
    lretq
.flush:
    ret

.global tss_flush
tss_flush:
    mov $0x28, %ax
    ltr %ax
    ret

.extern exception_handler

.macro isr_err_stub num
.global isr\num
isr\num:
    pushq $\num
    jmp isr_common_stub
.endm

.macro isr_no_err_stub num
.global isr\num
isr\num:
    pushq $0
    pushq $\num
    jmp isr_common_stub
.endm

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
isr_no_err_stub 30
isr_no_err_stub 31

isr_common_stub:
    push %r15
    push %r14
    push %r13
    push %r12
    push %r11
    push %r10
    push %r9
    push %r8
    push %rbp
    push %rdi
    push %rsi
    push %rdx
    push %rcx
    push %rbx
    push %rax

    mov %ds, %ax
    push %rax
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    mov %rsp, %rdi
    call exception_handler

    pop %rax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    pop %rax
    pop %rbx
    pop %rcx
    pop %rdx
    pop %rsi
    pop %rdi
    pop %rbp
    pop %r8
    pop %r9
    pop %r10
    pop %r11
    pop %r12
    pop %r13
    pop %r14
    pop %r15

    add $16, %rsp
    iretq


.global isr_keyboard
isr_keyboard:
    push %rax
    push %rcx
    push %rdx
    push %rdi
    push %rsi
    push %r8
    push %r9
    push %r10
    push %r11
    cld
    call keyboard_handler
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rsi
    pop %rdi
    pop %rdx
    pop %rcx
    pop %rax
    iretq
