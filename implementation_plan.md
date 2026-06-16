# MS-DOS LISP Edition - Plano de Implementação (Fase 1)

O objetivo final é criar um sistema operacional do zero, inspirado no MS-DOS 2.0, mas portável para x86-64 e Raspberry Pi (ARM), contendo um interpretador Common LISP nativo. Como este é um projeto colossal, este plano foca na **Fase 1: Infraestrutura de Boot e Kernel Básico (Bare-Metal)**.

## User Review Required
> [!WARNING]
> O desenvolvimento de Sistemas Operacionais é complexo e exige ferramentas específicas. Será necessário configurar um ambiente de compilação cruzada (Cross-Compiler) e um emulador (QEMU) para testar o código sem precisar formatar/reiniciar seu computador atual.

## Open Questions
> [!IMPORTANT]
> 1. **Ambiente de Desenvolvimento:** Para compilar um Sistema Operacional a partir do Windows, a melhor e mais fácil abordagem é usar o **WSL (Windows Subsystem for Linux)**. Você tem o WSL (como o Ubuntu) instalado no seu Windows? Se não, eu posso te guiar na instalação, ou podemos tentar usar ferramentas como LLVM/Clang nativas do Windows (embora dê mais trabalho).
> 2. **Foco Inicial:** Recomendo focarmos primeiro em fazer o Kernel dar boot na arquitetura **x86-64** (PC) usando o emulador QEMU, e estabelecer o sistema básico antes de nos preocuparmos em portar para o Raspberry Pi. Concorda com essa abordagem?
> 3. **Linguagem do Kernel:** Propomos usar a linguagem **C** padrão (com um pouquinho de Assembly quando estritamente necessário para falar com a CPU) por ser a mais documentada e clássica para desenvolvimento de SOs. Tudo bem para você?

## Proposed Changes
Criaremos a estrutura inicial do projeto na nova pasta `C:\Users\wsric\antigravity\MS-DOS`.
O sistema usará o bootloader moderno **Limine**, que facilita muito o processo de boot em PCs modernos (suportando tanto BIOS legado quanto UEFI), permitindo que foquemos direto na escrita do código em C do nosso Kernel.

### Estrutura Inicial do Projeto

#### [NEW] [README.md](file:///C:/Users/wsric/antigravity/MS-DOS/README.md)
Documentação inicial do projeto, objetivos e arquitetura LISP-DOS.

#### [NEW] [src/kernel/main.c](file:///C:/Users/wsric/antigravity/MS-DOS/src/kernel/main.c)
Ponto de entrada do nosso Kernel. Diferente de um programa normal que tem a função `main()` chamada pelo Windows, este arquivo terá a função principal que o bootloader chamará quando o PC ligar.

#### [NEW] [src/hal/vga.c](file:///C:/Users/wsric/antigravity/MS-DOS/src/hal/vga.c) e [vga.h](file:///C:/Users/wsric/antigravity/MS-DOS/src/hal/vga.h)
Drivers de abstração de hardware (HAL - Hardware Abstraction Layer). O driver VGA será responsável por acessar a memória de vídeo fisicamente e imprimir caracteres coloridos na tela, substituindo as velhas interrupções de BIOS do DOS original.

#### [NEW] [limine.cfg](file:///C:/Users/wsric/antigravity/MS-DOS/limine.cfg)
Arquivo de configuração do bootloader para que ele saiba como carregar nosso Kernel na memória.

#### [NEW] [Makefile](file:///C:/Users/wsric/antigravity/MS-DOS/Makefile)
Script que irá orquestrar a compilação do Kernel, a criação de uma imagem de disco ISO inicializável e a execução no emulador.

## Verification Plan

### Testes Manuais (Emulador)
Após a configuração, a verificação será:
1. Executar o comando de compilação/teste (`make run`).
2. O emulador **QEMU** deverá abrir uma janela separada simulando um computador ligando.
3. O bootloader Limine carregará nosso Kernel.
4. O sistema deverá imprimir de forma independente a mensagem "LISP-DOS (MS-DOS Clone) Iniciado com sucesso!" na tela, provando que nosso código está rodando direto no hardware (emulado), sem depender do Windows.
