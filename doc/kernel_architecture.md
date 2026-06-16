# Arquitetura do Kernel LISP-DOS

O LISP-DOS é um sistema operacional experimental baseado na arquitetura x86_64, desenvolvido com foco minimalista e projetado para carregar scripts LISP em Ring 3. O sistema usa o bootloader Limine via protocolo Multiboot2/Limine v8 e mapeia a memória virtual num layout HHDM (Higher Half Direct Mapping).

## 1. Gerenciamento de Memória

### Physical Memory Manager (PMM)
Localizado em `pmm.c`, o PMM é responsável por gerenciar páginas de RAM física de 4KB. Ele utiliza um *Bitmap* populado dinamicamente usando as informações do mapa de memória passado pelo bootloader Limine (`limine_memmap_request`). As páginas podem ser alocadas de forma unitária (`pmm_alloc_page`) ou contíguas (`pmm_alloc_pages`).

### Virtual Memory Manager (VMM)
Localizado em `vmm.c`, o VMM é um gerenciador de paginação de 4 níveis (PML4 -> PDPT -> PD -> PT). O Kernel reside nativamente em High Half (usando `hhdm_offset`), de modo que as páginas de sistema operacional são transparentemente sobrepostas na metade superior do endereço de 64 bits.

### Heap Allocator (kmalloc)
Localizado em `heap.c`, implementamos atualmente um *Bump Allocator Lazy*. Ao solicitar alocação de memória inferior a 4096 bytes via `kmalloc()`, o Kernel só efetua mapeamentos sob demanda a partir do PMM e "empurra" o ponteiro. Requisições maiores chamam diretamente blocos físicos contíguos (`pmm_alloc_pages`). Este método é rudimentar e suficiente para clones do DOS, pois dispensa estruturas complexas ou a implementação de uma macrofunção explícita no `heap_init()`.

## 2. Processos de Usuário e Carregamento ELF

Os binários de usuário (como `lisp.elf` e `editor.elf`) são arquivos compilados isoladamente para Ring 3 (`-fno-pic -fno-pie -no-pie`). O carregamento é realizado na função `process_exec` (em `process.c`):

1. Lê o executável do FAT16 ou TarFS.
2. Valida a integridade matemática do Header ELF (prevenção contra Buffer Over-read).
3. Verifica se o `e_entry` pertence ao User Space Ring 3, protegendo o Kernel de Hijacks.
4. Cria um Page Map independente e aloca stack limpa na região virtual `0x700000000000`.
5. Transfere a execução para Ring 3 via contexto seguro em Assembly (`sysretq`).

### Passagem de Argumentos (Command Tail)
A nossa arquitetura adota uma abordagem simples e robusta alinhada à ABI do C no x86_64 para passagem de argumentos da linha de comando:

- O Shell (`main.c`) analisa o comando, divide string-base (o nome do arquivo executável) do restante dos argumentos.
- O Kernel copia a string inteira de argumentos diretamente para a memória topo do *User Stack* recém-alocado.
- O ponteiro para a string (ainda na Stack) é passado no registrador `%rdx` para a macro Assembly de transição de contexto, que por sua vez movimenta a string para o `%rdi`.
- Quando o `sysretq` dispara a execução do processo no Ring 3, o binário já interpreta naturalmente a assinatura `void _start(const char* args)`.

## 3. Troca de Contexto e Syscalls (Ring 0 <-> Ring 3)

O LISP-DOS suporta escalonamento manual usando a instrução x86_64 `syscall`/`sysret`.

- **Ida ao Ring 3 (`asm_jump_usermode` em `syscall_asm.S`):** O processo é ativado preservando explicitamente na variável `kernel_saved_rsp` a base da Stack original do Kernel e realizando backup em stack de todos os *Callee-Saved Registers* (`RBP`, `RBX`, `R12-R15`).
- **Retorno de Interrupção (`asm_return_to_kernel` em `syscall_asm.S`):** Chamado via `sys_exit` (Syscall 60) ou falhas, ele resgata a pilha do kernel (`kernel_saved_rsp`), limpa as variáveis e resgata obrigatoriamente a flag de *Interrupt Flag (IF)* usando a instrução `sti` para impedir que o teclado trave após devolução da interrupção gerada pelo MSR_FMASK.
- **Syscall Handler (`syscall_entry`):** O wrapper em Assembly é responsável por realizar a ponte entre o comando de Ring 3 e a ABI de Syscalls (System V). Foi essencial projetar a salvaguarda de todos os registradores temporários voláteis (como `%r8` e `%r9`) dando `push` e `pop` transparentemente, pois o C/Kernel os corrompe por padrão, o que causava temidos *Page Faults* por desvio de endereçamento no Ring 3.
- **Deadlocks e Hardware I/O:** Syscalls bloqueantes (como capturar inputs de teclado no `keyboard.c`) não podem executar instruções `hlt` cegamente. Como a CPU reseta o Interrupt Flag em Syscalls por segurança (impedindo IRQs locais), o Kernel agora gerencia ativamente o gatilho religando as interrupções usando a dupla `sti` seguida atomicamente de `hlt` para dormir de maneira performática, prevenindo Race Conditions ou congelamentos permanentes de CPU.
