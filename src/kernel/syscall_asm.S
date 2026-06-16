.global syscall_entry
.extern syscall_handler_c

.extern current_user_rsp
.extern current_kernel_stack

syscall_entry:
    // Salvar o RSP do usuario e trocar para o RSP do kernel
    mov %rsp, current_user_rsp(%rip)
    mov current_kernel_stack(%rip), %rsp
    
    // %rcx contém RIP de retorno (sysret usa rcx)
    // %r11 contém RFLAGS (sysret usa r11)
    
    push %rcx
    push %r11
    
    // Argumentos Syscall ABI do Linux (x86_64):
    // RDI, RSI, RDX, R10, R8, R9
    // syscall num em RAX
    
    push %r10
    push %r9
    push %r8
    push %rdx
    push %rsi
    push %rdi
    push %rax
    
    mov %rax, %rdi
    mov 8(%rsp), %rsi
    mov 16(%rsp), %rdx
    mov 24(%rsp), %rcx
    
    call syscall_handler_c
    
    // O valor de retorno esta em RAX. Vamos descartar o RAX salvo
    add $8, %rsp
    
    pop %rdi
    pop %rsi
    pop %rdx
    pop %r8
    pop %r9
    pop %r10
    
    pop %r11
    pop %rcx
    
    mov current_user_rsp(%rip), %rsp
    
    sysretq

.global asm_jump_usermode
.global asm_return_to_kernel

.data
kernel_saved_rsp: .quad 0

.text
asm_jump_usermode:
    // RDI = entry_point, RSI = user_stack, RDX = args_string
    
    // Salva registradores callee-saved do kernel
    push %rbp
    push %rbx
    push %r12
    push %r13
    push %r14
    push %r15
    
    // Salva o RSP atual no kernel_saved_rsp
    mov %rsp, kernel_saved_rsp(%rip)
    
    // Prepara registradores para o sysret
    mov %rdi, %rcx       // RIP = entry_point
    mov %rsi, %rsp       // RSP = user_stack
    mov %rdx, %rdi       // RDI = args_string (primeiro argumento para o C)
    mov $0x202, %r11     // RFLAGS (com IF=1)
    
    sysretq

asm_return_to_kernel:
    // Restaura o RSP do momento do jump_usermode
    mov kernel_saved_rsp(%rip), %rsp
    
    // Restaura os registradores callee-saved
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %rbx
    pop %rbp
    
    // Reabilita as interrupcoes (foram desativadas pela syscall/FMASK)
    sti
    
    // Retorna para process_exec
    ret
