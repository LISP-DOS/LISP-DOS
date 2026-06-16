# LISP-DOS Architecture and Walkthrough

## 1. O Problema Crítico no Boot (Limine HHDM)
O sistema estava apresentando a falha **"Erro: Limine nao forneceu o HHDM"**.
Após uma análise arquitetural profunda de como o projeto interagia com o bootloader Limine, foi identificada a seguinte falha de projeto:

### A Causa Raiz
O projeto estava utilizando um padrão de _array_ de ponteiros (`limine_requests[]`) entre delimitadores (`LIMINE_REQUESTS_START_MARKER` e `LIMINE_REQUESTS_END_MARKER`). 
Este padrão é do protocolo do Limine **Base Revision 0** e foi **depreciado e completamente removido** na **Base Revision 1** (versão suportada pelo Limine v8 que estamos usando).
Como resultado, o Limine ignorava completamente os ponteiros do array e procurava pelos identificadores mágicos (structs) apenas dentro da região delimitada. Porém, as _structs_ de _requests_ (como `hhdm_request`, `framebuffer_request`, etc) estavam sendo alocadas fora dessa região pelo linker (em `.data`), fazendo com que o Limine não encontrasse as requisições e não as processasse, mantendo seus ponteiros `response` como `NULL`.

### A Solução
Removida a arquitetura obsoleta de ponteiros. Adicionado a flag `__attribute__((used, section(".requests")))` diretamente na declaração de todas as _structs_ do Limine pelo kernel:
* `limine_base_revision` (main.c)
* `framebuffer_request` (vga.c)
* `module_request` (tarfs.c)
* `memmap_request` & `hhdm_request` (pmm.c)
* `rsdp_request` (acpi.c)

Com esta adequação ao novo padrão estrutural do Limine v8, o HHDM e todos os outros módulos (Memória, Framebuffer e ACPI/RSDP) passaram a ser mapeados corretamente, permitindo que o kernel suba normalmente até a montagem do disco e chamadas ao shell.

## 2. Isolamento de Código (Userspace)
Conforme solicitado, separamos ferramentas e aplicativos de usuário do núcleo principal:
* `editor.c` e `lisp.c` foram movidos para a pasta `src/tools/`.
* Seus binários (`editor.elf` e `lisp.elf`) agora são injetados diretamente na imagem HHD (fat16) durante a etapa de build, atuando 100% como programas apartados do Kernel via syscalls (syscall `sys_exec`).

## 3. Gestão de Energia (`poweroff` e comando `exit`)
Para implementar a capacidade da máquina real (e do QEMU) ser completamente desligada pelo comando `exit` ou `poweroff`:
* Foi adicionado mapeamento em `acpi.c` para ler a estrutura **FADT (Fixed ACPI Description Table)** e localizar o endereço de gerenciamento PM1a.
* O comando `sys_poweroff` interage diretamente com as portas ACPI (`outw`) sinalizando à placa-mãe o evento _Sleep Type 5_ (`S5`), resultando no desligamento físico.
* O shell foi atualizado para reconhecer tanto `exit` quanto `poweroff` para iniciar este desligamento de forma segura.
