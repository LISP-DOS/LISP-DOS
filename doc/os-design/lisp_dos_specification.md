# LISP-DOS: Especificações e Guia de Desenvolvimento

Este documento centraliza a arquitetura, o design e as instruções de manutenção para o **LISP-DOS**, um sistema operacional educacional construído em C, rodando nativamente na arquitetura x86_64, que conta com um interpretador LISP embutido e um editor de textos estilo Vi.

## 1. Visão Geral da Arquitetura
O LISP-DOS adota um design de kernel monolítico simples, dividido primariamente entre o **Core do Kernel** e a **Camada de Abstração de Hardware (HAL)**.

- **Bootloader:** Limine (suporta inicialização Híbrida: BIOS Legacy e UEFI).
- **Arquitetura Alvo:** x86_64 (64-bits).
- **Linguagem Principal:** C (com algumas rotinas de interrupção em Assembly).
- **Binário do Kernel:** Compilado como arquivo ELF estático e linkado via `linker.ld`.

## 2. Estrutura de Diretórios e Componentes

### 2.1 Camada de Hardware (HAL - `src/hal/`)
Responsável pela comunicação direta com o hardware da máquina.
- **`vga.c` / `font8x8_basic.h`:** Driver de vídeo. Gerencia a renderização de texto na tela. Dependendo de como o Limine fornece o framebuffer, ele desenha as fontes manualmente ou usa o modo texto da VGA (`0xB8000`).
- **`gdt.c` / `idt.c` / `pic.c`:** Configuração da CPU. 
  - GDT (Global Descriptor Table) para segmentação de memória.
  - IDT (Interrupt Descriptor Table) e PIC (Programmable Interrupt Controller) para lidar com exceções da CPU e interrupções de hardware (teclado, timer).
- **`interrupts.S`:** Stubs em Assembly para salvar e restaurar o estado dos registradores durante o tratamento de interrupções.
- **`keyboard.c`:** Driver do controlador de teclado PS/2 (Interrupção IRQ1).
- **`ata.c`:** Driver de disco PIO para ler/escrever em discos IDE/ATA.
- **`acpi.c` / `pci.c` / `usb.c`:** Inicialização do barramento PCI, tabelas ACPI para desligamento físico (S5 state), e infraestrutura básica (stubs) para os futuros drivers xHCI/EHCI nativos.
- **`io.h`:** Funções inline para instruções `inb`, `outb`, etc. (Comunicação com portas de I/O do x86).

### 2.2 Core do Kernel (`src/kernel/`)
- **`main.c`:** Ponto de entrada do Kernel. Inicializa a HAL, monta os sistemas de arquivos e inicia o Shell.
- **`pmm.c` / `heap.c`:** Gerenciamento de Memória.
  - `pmm`: Physical Memory Manager (Gerenciador de páginas físicas de 4KB usando o mapa de memória passado pelo Limine).
  - `heap`: Implementação de `malloc` e `free` para alocação dinâmica dentro do kernel.
- **Sistemas de Arquivos:**
  - **`tarfs.c`:** Driver de sistema de arquivos *Initramfs* baseado em formato TAR (geralmente usado como unidade C: apenas leitura com os utilitários básicos).
  - **`fat16.c`:** Driver de sistema de arquivos FAT16 (usado no disco rígido simulado `hdd.img` ou físico para persistir arquivos editados).
- **Processos e Syscalls (Ring 3):**
  - **`syscall.c` / `syscall_asm.S`:** Implementação das Syscalls padrão do Linux via MSRs e instrução `syscall`/`sysret` para interação com o Ring 3.
  - **`process.c` / `elf.h`:** Carregador de binários ELF (Executable and Linkable Format). Lê o binário ELF do disco, mapeia segmentos em Ring 3 na memória virtual do processo via VMM, e executa os programas de usuário.

### 2.3 Ferramentas e Aplicativos do Usuário (`src/tools/`)
Aplicativos do usuário são totalmente isolados do núcleo do sistema e são construídos como executáveis genéricos `.elf`. Eles não contêm mais cabeçalhos do Kernel. Em vez disso, se comunicam com o Kernel através da System Call ABI.
  - **`syscalls.h`:** Biblioteca C minimalista do usuário fornecendo funções wrapper (`sys_write`, `sys_clear`, `sys_fat16_read_file`, `sys_exit`) para fazer a ponte das Syscalls via instrução Assembly `syscall`.
  - **`lisp.elf` (`lisp.c`):** Interpretador LISP compilado como aplicativo de usuário.
  - **`editor.elf` (`editor.c`):** Editor de textos nativo no terminal, com modos de Inserção e Normal.

## 3. ABI de Syscalls e Gerenciamento de Processos
A partir da versão estruturada, o sistema conta com Syscalls no padrão System V ABI (usando RAX, RDI, RSI, RDX, R10).
- **Segurança de Ring 3 (Stack Swap):** Foi solucionada a vulnerabilidade de Page Fault por Pilha de Usuário. Ao realizar uma chamada via `syscall`, a instrução `swapgs` (ou salvamento global) garante que os argumentos sejam armazenados em uma pilha exclusiva e controlada do Kernel (`kernel_stack_top`).
- **Memory Leak Protection:** Quando o aplicativo invoca a Syscall `sys_exit` (Num 60), o LISP-DOS automaticamente liberta todas as tabelas de paginação locais (`vmm_free_pagemap`) e devolve os endereços físicos associados ao ELF para o Physical Memory Manager (PMM), garantindo que não há vazamento de memória ram em execuções repetidas de ferramentas.

## 3. Ambiente de Construção (Build System)

### Pré-requisitos
- **GCC / Binutils** (Cadeia de ferramentas multilib ou target x86_64-elf).
- **Make**.
- **Xorriso** (Para gerar a ISO bootável).
- **QEMU** (Para emulação).

### Comandos do Makefile
- `make` ou `make all`: Compila o código-fonte (`.c` e `.S`) para arquivos objeto (`.o`) e linka o kernel usando `linker.ld`.
- `make iso`: Gera a imagem `msdos-lisp.iso` inserindo o kernel compilado e as pastas do initramfs através do utilitário Limine.
- `make hdd.img`: Compila programas de teste ELF (como `test.elf`) e injeta no disco FAT16 de imagem de disco `hdd.img` usando a ferramenta Linux `mcopy`.
- `make run`: Compila e sobe o sistema rapidamente no **QEMU**, geralmente com opções como `-cdrom msdos-lisp.iso -drive file=hdd.img,format=raw,if=ide`.

## 4. Diretrizes de Desenvolvimento e Manutenção

### 4.1 Modificando o Kernel
- **Não use bibliotecas padrão (`<stdio.h>`, `<stdlib.h>`).** O kernel roda em modo *freestanding* (sem OS por baixo dele). Você deve usar as suas próprias implementações (ex: o seu `malloc` definido no `heap.c` e funções de I/O como `printf` customizadas que escrevem na VGA).
- **Isolamento de Stack:** Cada programa em Ring 3 recebe sua própria stack física virgem gerada no runtime por `pmm_alloc_page()` e referenciada virtualmente no `process_pml4`.
- **Validação Restrita de ELF (Anti-Hijack):** O LISP-DOS não confia no binário executado. O sistema conta os limites das diretivas ELF (`e_phoff`) para impedir leituras além do arquivo e aborta execuções onde o `e_entry` tenta pular para ponteiros de espaço Top-Half de Kernel.
- **Rollback de HD (FAT16):** A HAL do VFS previne Memory Leaks definitivos de clusters no HD. Se uma escrita por `editor.elf` explodir o limite do disco rígido local, as cadeias quebradas são varridas de volta e esvaziadas (limpas para 0x0) em fallback, mantendo a tabela FAT intocada.
- Sempre verifique alocações de memória, visto que não há um SO para limpar se houver vazamentos (*memory leaks*) nas ferramentas nativas (LISP/Editor).

### 4.2 Adicionando Novos Comandos no Shell
1. O loop principal do shell geralmente fica no `main.c`.
2. Comandos atuais suportados nativamente:
   - `cls` ou `clear`: Limpa a tela.
   - `dir`: Lista arquivos no Initramfs e no disco FAT16.
   - `edit <arquivo>`: Abre o editor de textos.
   - `lisp <arquivo>`: Executa um script LISP.
   - `echo <texto>`: Imprime um texto na tela.
   - `type <arquivo>`: Mostra o conteúdo de um arquivo do Initramfs.
   - `exec <arquivo>`: Carrega e executa um binário no formato ELF isolado no espaço de usuário (Ring 3).
   - `reboot`: Reinicia a máquina via controlador de teclado 8042.
   - `poweroff` / `exit`: Sincroniza dados com a RAM, encerra os buffers e desliga fisicamente a máquina real via ACPI.
3. Para adicionar um comando, crie uma condicional validando o buffer do teclado (ex: `strcmp(cmd, "meu_comando") == 0`).
4. Chame a função desejada.

### 4.3 Trabalhando com o Disco
Atualmente, o sistema suporta **FAT16**.
- Ao criar ou salvar novos scripts através do `editor.c`, as chamadas passam pelo `fat16.c` para gravar fisicamente os setores via `ata.c`.
- Se você criar arquivos no Windows/Linux, é possível injetá-los na imagem `hdd.img` usando ferramentas de manipulação de disco (como `mtools` ou montagem em loop) para que o LISP-DOS os leia.

## 5. Testando em Hardware Real

Caso queira homologar versões fora do QEMU:
1. Gere a imagem ISO (`make iso`).
2. Utilize o utilitário **Rufus** (Windows) ou **dd** (Linux) para gravar a ISO `msdos-lisp.iso` em um pendrive.
3. Configure o BIOS/UEFI do computador alvo para iniciar a partir do pendrive.
4. *Atenção:* O teclado USB necessita da opção "Legacy USB Support" ativada na placa-mãe para funcionar com o driver PS/2 (`keyboard.c`). Discos rígidos físicos precisam estar em modo IDE para o `ata.c` reconhecê-los (se for usar o HD em vez do Initramfs CD-ROM).
