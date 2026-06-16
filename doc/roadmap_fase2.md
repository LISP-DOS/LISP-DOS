# LISP-DOS: Roadmap de Arquitetura e Planejamento da Fase 2

Este documento traça o diagnóstico técnico da recém-encerrada **Fase 1** e propõe o mapa de desenvolvimento estruturado para a **Fase 2** e além.

## Análise Profunda e Pontos de Melhoria (Limitações Atuais)
Embora a Fase 1 tenha nos dado um Kernel Ring 0/Ring 3 perfeitamente seguro e isolado, ele possui limitações arquiteturais características de sistemas primitivos que impedirão que ele rode nativamente em *Hardware Moderno* puro (bare-metal):

1. **Dependência de PS/2 (Emulação):** O driver de teclado atual escuta a porta de I/O `0x60` (IRQ 1). Computadores modernos (sem emulação Legacy BIOS) usam teclados nativos USB, tornando o sistema inoperante fora de VMs se não tivermos um stack USB.
2. **I/O de Disco Lento (ATA PIO):** O disco lê e escreve transferindo blocos 512 bytes via CPU (portas I/O). É extremamente lento e bloqueante. Precisamos evoluir para discos SATA modernos usando **AHCI e DMA** (Direct Memory Access).
3. **Falta de Relógio (RTC/PIT):** O LISP-DOS não tem noção do tempo. Não há instrução de "Sleep" e nem carimbos de data/hora nos arquivos FAT16.
4. **Limitações do FAT16:** Atualmente o driver FAT16 lê e escreve apenas no Diretório Raiz. Subdiretórios (ex: `C:\LISP\SCRIPTS`) não são suportados.

---

## O Plano Estratégico (Visão Geral)

A **Fase 2** será totalmente focada em expandir as fundações de Hardware (Drivers e I/O).
As Fases seguintes (3, 4 e 5) focarão na experiência do usuário, sistemas de arquivos abstratos e gráficos avançados.

---

## Detalhamento da "Fase 2: Expansão de Hardware e I/O"

Para facilitar o desenvolvimento, a Fase 2 foi subdividida em três grandes marcos (Sprints).

### Parte 2.1: Tempo e Relógio do Sistema (RTC & PIT)
A primeira infraestrutura essencial para qualquer OS.
- **Desenvolvimento:** Criar drivers para o Real Time Clock (RTC) para obtermos a Data e Hora real do mundo, e programar o Programmable Interval Timer (PIT) para ticks do Kernel.
- **Integração:** Adicionar Syscalls de Time (`sys_time`, `sys_sleep`) para que as aplicações Ring 3 (LISP) possam medir performance de algoritmos ou dormir sem travar a CPU.

### Parte 2.2: O Subsistema PCIe e USB nativo (xHCI)
A tarefa mais desafiadora e vital para rodar em hardware contemporâneo.
- **Desenvolvimento:** 
  1. Escrever um scanner de barramento PCIe para enumerar os dispositivos físicos plugados na placa mãe.
  2. Identificar a controladora USB 3.0 (xHCI).
  3. Criar a base do driver `xhci.c` configurando os anéis de comunicação (Command/Transfer Rings).
- **Integração:** Escrever o driver de Teclado USB HID operando puramente por pacotes assíncronos, abandonando as portas I/O antiquadas.

### Parte 2.3: Aceleração de Disco com SATA AHCI
Desbloquear alta velocidade de leitura/escrita.
- **Desenvolvimento:** Através do barramento PCIe criado na etapa anterior, encontrar o controlador SATA (AHCI).
- **Integração:** Substituir ou integrar o driver atual de `ata.c` por `ahci.c` usando endereçamento PRDT (Physical Region Descriptor Table) para transferir dados direto do HD para a Memória RAM sem pesar no processador.

---

## Vislumbre das Fases Futuras

### Fase 3: Maturidade de Sistema de Arquivos (VFS e FAT32)
- Sistema hierárquico completo (suporte a pastas e navegação `cd`).
- Camada VFS (*Virtual File System*) implementando o conceito UNIX de descritores (File Descriptors) com `sys_open`, `sys_read`, `sys_close`.
- Adição de FAT32 para suportar PenDrives e partições acima de 2GB.

### Fase 4: Ambientes Gráficos e Mouse (Modo GUI)
- Transição da filosofia do LISP-DOS do "Modo Texto" para uma API de Janelas ou gráficos de alta resolução.
- Primitivas gráficas no LISP (desenhar linhas, círculos, plotar matrizes matemáticas visualmente).
- Driver nativo de Mouse USB na interface xHCI criada na Fase 2.

### Fase 5: Multitarefa Preemptiva (Multitasking)
- Modificar o escalonador Single-Thread atual para suportar múltiplos programas em paralelo no Ring 3.
- Timer Interrupts forçando trocas de contexto (*Context Switch*) transparentes entre o shell e scripts em background.

---

## User Review Required

> [!IMPORTANT]
> Este documento desenha o escopo de todo o futuro do LISP-DOS. Você concorda com a organização técnica da **Fase 2** priorizando Relógio -> PCIe/USB -> AHCI SATA?
> Deseja que eu inicialize a **Parte 2.1 (Timers)** imediatamente na pasta fonte, ou você prefere pularmos direto para a **Parte 2.2 (USB xHCI)**, que é o seu objetivo de longa data?
