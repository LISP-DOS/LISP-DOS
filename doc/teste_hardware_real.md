# Testando o LISP-DOS em Hardware Real

Para testar o seu sistema operacional rodando em uma máquina real, o processo é bastante direto, já que o Limine já está gerando uma imagem `.iso` híbrida (que suporta tanto BIOS legado quanto UEFI).

Aqui estão os passos que você precisa seguir:

## 1. Gravar a ISO em um Pendrive
Você precisa transferir a imagem ISO (`msdos-lisp.iso` gerada pelo comando `make run` ou `make iso`) para um pendrive de forma "bootável". 

**No Windows:**
Você pode usar um programa como o **Rufus** ou **BalenaEtcher**.
- Baixe e abra o Rufus.
- Selecione o seu pendrive.
- Em "Seleção de boot", escolha o arquivo `msdos-lisp.iso`.
- O Rufus provavelmente detectará que é uma imagem híbrida (ISOHybrid). Se perguntar, escolha gravar em modo DD (ou apenas siga o padrão recomendado).
- Clique em Iniciar (todos os dados do pendrive serão apagados).

**No Linux/WSL (via linha de comando):**
Se o pendrive estiver mapeado no seu Linux (por exemplo, em `/dev/sdX`), você pode usar o comando `dd`:
```bash
sudo dd if=msdos-lisp.iso of=/dev/sdX bs=4M status=progress
```
*(Tenha muito cuidado para não errar a letra do drive, pois isso pode apagar o seu disco rígido principal).*

## 2. Dar Boot pela Máquina Real
1. Insira o pendrive na máquina em que você deseja testar.
2. Ligue a máquina e entre no menu da **BIOS/UEFI** (geralmente pressionando teclas como `F2`, `F12`, `Delete` ou `Esc` logo que a máquina liga).
3. Vá até a seção de **Boot** e mude a ordem de inicialização para que o "USB Storage Device" (ou o nome do seu pendrive) seja o primeiro da lista. Alternativamente, muitas placas-mãe têm um "Boot Menu" rápido (ex: `F12`) onde você pode selecionar o pendrive diretamente para aquela inicialização.
4. Salve as alterações e reinicie.

## 3. O que esperar (Limitações de Hardware Real)
Rodar em hardware real é o maior teste para um sistema operacional próprio. Algumas coisas podem funcionar no QEMU, mas falhar na máquina real:

- **Teclado:** Atualmente, o seu sistema operacional provavelmente usa um driver de teclado PS/2 (`src/hal/keyboard.c`). Muitos computadores modernos (laptops, placas-mãe novas) não têm mais controladora PS/2. O seu teclado USB só vai funcionar se a BIOS/UEFI da sua máquina tiver a opção de **"USB Legacy Support"** ou **"PS/2 Emulation"** ativada (o que permite que a placa-mãe finja que o teclado USB é um PS/2 para sistemas operacionais antigos).
- **Disco Rígido (FAT16 / ATA):** O driver ATA (`src/hal/ata.c`). Placas-mãe modernas usam AHCI ou NVMe nativamente. O seu driver ATA legado só funcionará se a BIOS tiver suporte ao modo IDE/Compatibility para os controladores SATA. Para ler pendrives USB mass storage nativamente, você precisaria de um driver USB.
- **Vídeo (VGA):** Como o Limine lida com a tela, se o seu kernel usa texto puro (modo VGA `0xB8000`) ou um framebuffer fornecido pelo Limine, ele tem grandes chances de funcionar perfeitamente logo de cara!
- **UEFI vs BIOS Legacy:** O Limine gerencia muito bem as diferenças. Se a sua máquina for UEFI (computadores de 2012 para cá), o Limine será carregado em modo UEFI e fará o boot do seu Kernel.
