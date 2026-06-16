# LISP-DOS

Um sistema operacional feito do zero, focado em um interpretador Common LISP nativo em arquitetura x86_64.

## Fase 1: Boot e Kernel Básico

A infraestrutura inicial do sistema roda em bare-metal e foi desenhada para a arquitetura x86-64 utilizando o bootloader Limine.

### Compilação e Execução

Para compilar, você precisará estar em um ambiente Linux (ou WSL no Windows) com as seguintes dependências instaladas:
- `build-essential` (make, gcc)
- `mtools`
- `xorriso` (para criar a imagem ISO)
- `qemu-system-x86` (para rodar o emulador)

Comandos úteis:
- `make` - Compila o Kernel e gera a imagem `msdos-lisp.iso`.
- `make run` - Compila e abre a imagem no QEMU.
- `make clean` - Limpa os arquivos compilados.
