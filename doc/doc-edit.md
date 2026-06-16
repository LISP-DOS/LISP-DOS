# LISP-DOS Editor: Documentação de Implementação

O `editor.c` (compilado para `bin/editor.elf`) é o aplicativo de edição de texto nativo do LISP-DOS. Semelhante ao Vi/Vim, ele possui dois modos principais: o **Modo Normal** (para navegação e acionamento de comandos) e o **Modo Inserção** (para digitação).

Após as recentes atualizações arquiteturais, o editor passou a atuar como um processo isolado em espaço de usuário (Ring 3), comunicando-se com o kernel via Syscalls (`syscalls.h`), e deixando de ser estaticamente linkado como uma função nativa do kernel.

## Arquivos Relacionados

### `editor.h`
Historicamente fornecia o protótipo da função de invocação (`void editor_start(const char* filename);`). Atualmente, serve de artefato ou referência, dado que o processo em Ring 3 agora utiliza `_start` como seu ponto natural de entrada.

### `editor.c`
Contém a lógica integral do aplicativo, sendo responsável por renderizar a "tela", administrar o estado dos modos de operação (Insert/Normal) e realizar as chamadas de gravação em disco.

---

## Variáveis Globais de Estado

Como um executável Ring 3, as variáveis de estado do editor residem na memória virtual dedicada ao processo:

- `edit_buffer[EDIT_BUFFER_SIZE]`: Um array de 2048 bytes onde o texto sendo editado fica temporariamente armazenado na memória RAM antes de ser salvo. Funciona como o "canvas" do usuário.
- `buffer_len`: A quantidade de caracteres válidos preenchidos no buffer.
- `cursor_pos`: Posição lógica de 1D em que o cursor está parado no momento dentro do `edit_buffer`.
- `is_insert_mode`: Flag condicional (0 = Normal, 1 = Insert) que roteia a interpretação do teclado.
- `current_filename`: Armazena a string do nome do arquivo (ex: `SOMA.LI`) com até 32 bytes de limite.
- `last_cmd_key`: Em modo normal, registra a última tecla apertada para feedback visual na barra de status.

---

## Funções e Lógica Algorítmica

### `draw_editor_ui(void)`
Essa rotina atualiza completamente o framebuffer de texto via Syscalls (ex. `vga_clear`). A lógica funciona assim:
1. Limpa todo o conteúdo residual da tela.
2. Posiciona o cursor virtual na parte inferior para desenhar a **Barra de Status** ("-- INSERT --" ou "-- NORMAL --").
3. Volta à linha `0, 0` e começa a iterar sob os caracteres de `edit_buffer`.
4. **Desenhando o Cursor:** A sacada gráfica desta função está no `if (i == cursor_pos)`. Quando a iteração alcança o índice onde o cursor está apontado, o sistema executa `vga_set_color(0, 2)` (inversão de cor para texto preto com fundo verde escuro) para renderizar a letra (ou o espaço em branco) na tela simulando o "bloco maciço" do cursor piscante. Terminada a letra, a cor é restaurada para a padrão.

### `kstrcpy(char* dest, const char* src)`
Função utilitária e minimalista para realizar a cópia estática do nome do arquivo da memória para a variável global `current_filename`.

### `_start(const char* args)` (Entry Point / Loop de Eventos)
A função `_start` serve como o `main()` desse programa binário independente.

1. **Setup Inicial**: Carrega os argumentos do usuário. Caso você insira `edit arquivo.txt`, ele copia esse parâmetro. Se o tamanho da requisição pelo `fat16_read_file` for positivo (sinal de que ele já existia no disco), copia esse conteúdo para a variável `edit_buffer`.
2. **Infinite Event Loop**: Trava a CPU do processo chamando ininterruptamente a interrupção bloqueante `keyboard_getchar()`.

#### Tratamento de Movimentação do Cursor (Setas Direcionais 128-131)
- **Avançar/Retornar**: Direita aumenta e esquerda diminui `cursor_pos`, nunca ultrapassando os limites (0 a `buffer_len`).
- **Navegação Vertical (Linhas):** Para subir e descer linhas logicamente num array 1D (`edit_buffer`), o algoritmo retrocede iterativamente o cursor achando o final/começo da última quebra de linha `\n`, calcula o número da coluna visual (`col`) do usuário e busca restaurar o X e Y da navegação aplicando os mesmos pulos em posições subsequentes no array.

#### Tratamento no Modo Inserção (`is_insert_mode == 1`)
Toda tecla digitável injeta caracteres no editor.
- **Backspace (`\b`):** Aplica um "Shift-Left" sequencial no array 1D a partir do `cursor_pos` atual, sobrescrevendo a posição do cursor e decrementando o tamanho.
- **Inserir Caractere**: Pega todo o texto a partir do cursor e arrasta um bloco para a frente ("Shift-Right") dentro de um for loop inverso (`for(int i = buffer_len; i > cursor_pos; i--)`). Isso abre "espaço" no índice atual para o novo caractere entrar sem quebrar o resto do script.

#### Tratamento no Modo Normal (`is_insert_mode == 0`)
O modo Vi, lendo atalhos rápidos:
- **`i` (Insert):** Passa o sistema para estado de inserção e atualiza a UI.
- **`w` (Write / Salvar):** Sincroniza a memória. Aciona a Syscall que se conecta ao `fat16_write_file` injetando todo o `edit_buffer` persistindo no Disco ATA, reportando falhas. Em seguida, desfaz as dependências desse programa chamando a Syscall de encerramento (`sys_exit()`).
- **`q` (Quit / Sair):** Pede o desligamento do programa (`sys_exit()`) sem aplicar um Write.

Qualquer tecla ESC em ambos os modos forçará o estado `is_insert_mode` para 0.
