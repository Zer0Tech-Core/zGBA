# Documento Técnico de Referência: Subsistema de Vídeo (PPU) do Game Boy Advance (zGBA)

**Versão:** 1.0
**Propósito:** Especificação completa do subsistema de vídeo (Picture Processing Unit - PPU) do Game Boy Advance para a implementação do emulador zGBA. Este documento detalha a arquitetura gráfica, os modos de vídeo, as estruturas de dados (tiles, mapas, sprites), os efeitos especiais e o pipeline de renderização.

## 1. Introdução ao Subsistema de Vídeo

O Game Boy Advance (GBA) possui um **Picture Processing Unit (PPU)** que é uma evolução significativa em relação ao Game Boy Color, incorporando muitas características do Super Nintendo (SNES), como suporte a múltiplas camadas de fundo, transformações afim (rotação e escala) e sprites com tamanhos variáveis.

O PPU do GBA é responsável por gerar o sinal de vídeo para o display LCD de 240x160 pixels, com uma profundidade de cor de 15 bits (32.768 cores). Ele opera a uma taxa de atualização de aproximadamente 59.73 Hz (280.896 ciclos de CPU por quadro).

### 1.1. Especificações da Tela

- **Resolução:** 240 x 160 pixels
- **Profundidade de Cor:** 15-bit (5 bits por canal R, G, B) -> 32.768 cores
- **Taxa de Atualização:** ~59.73 Hz (280.896 ciclos de CPU)
- **Pipeline de Renderização:**
    - **VDraw:** 160 scanlines de desenho ativo.
    - **VBlank:** 68 scanlines de período de blanking vertical.
    - **HDraw:** ~1004 ciclos por scanline (desenho ativo).
    - **HBlank:** ~228 ciclos por scanline (blanking horizontal).

## 2. Memória de Vídeo e Estruturas de Dados

O PPU do GBA acessa várias regiões de memória para compor a imagem final. A tabela abaixo resume essas regiões:

| Região | Endereço | Tamanho | Largura | Propósito |
| :--- | :--- | :--- | :--- | :--- |
| **VRAM** | `0x0600_0000` | 96 KB | 16-bit | Armazena tiles, mapas de tela, dados de sprites e framebuffers. |
| **OAM** | `0x0700_0000` | 1 KB | 32-bit | Object Attribute Memory. Armazena os atributos de até 128 sprites. |
| **Palette RAM** | `0x0500_0000` | 1 KB | 16-bit | Armazena paletas de cores para backgrounds e sprites. |

### 2.1. VRAM (Video RAM)

A VRAM de 96 KB é o coração do subsistema gráfico. Ela é dividida em várias regiões lógicas para diferentes propósitos:

- **Charblocks (Blocos de Caracteres):** A VRAM é organizada em "charblocks", que são blocos contínuos de 16 KB cada. Existem 6 charblocks no total (0 a 5), totalizando 96 KB.
    - **Charblocks 0-3:** Reservados para dados de background (tiles).
    - **Charblocks 4-5:** Reservados para dados de sprites (tiles).
- **Screenblocks (Blocos de Tela):** Cada screenblock tem 2 KB e define um mapa de tela de 32x32 tiles (cada tile é um índice de 16 bits). Vários screenblocks podem ser combinados para formar um background maior.
- **Tiles:** São bitmaps de 8x8 pixels. Podem ser:
    - **4 bpp (16 cores):** 32 bytes por tile.
    - **8 bpp (256 cores):** 64 bytes por tile.
- **Mapas de Tela:** Estruturas que organizam os tiles em uma grade para formar os backgrounds.

### 2.2. OAM (Object Attribute Memory)

A OAM de 1 KB armazena os atributos de até 128 sprites. Cada entrada de sprite tem 32 bits (4 bytes). A estrutura da OAM é a seguinte:

- **Atributo 0 (16 bits):**
    - **Bits 0-7:** Posição Y do sprite.
    - **Bits 8-9:** Tipo de sprite (0 = Normal, 1 = Afim, 2 = Desativado, 3 = Desativado).
    - **Bit 10:** Modo de blend (0 = Normal, 1 = Semi-transparente).
    - **Bit 11:** Espelhamento horizontal (para sprites normais).
    - **Bit 12:** Espelhamento vertical (para sprites normais).
    - **Bits 13-15:** Tamanho do sprite (forma e dimensão, ver tabela).
- **Atributo 1 (16 bits):**
    - **Bits 0-8:** Posição X do sprite.
    - **Bits 9-13:** Número do tile base (índice do primeiro tile na VRAM).
    - **Bits 14-15:** Prioridade do sprite (0 = mais alta, 3 = mais baixa).
- **Atributo 2 (16 bits):**
    - **Bits 0-7:** Número da paleta (para sprites 4 bpp) ou bits de cor (para sprites 8 bpp).
    - **Bits 8-9:** Modo de mosaic.
    - **Bits 10-11:** Parâmetro de blend.
    - **Bits 12-15:** Dados afim (para sprites com transformação afim).

### 2.3. Palette RAM

A Palette RAM de 1 KB é dividida em duas paletas de 256 entradas cada:

- **Paleta de Background:** Endereço `0x0500_0000`.
- **Paleta de Sprites:** Endereço `0x0500_0200`.

Cada entrada de paleta é um valor de 16 bits no formato `XBBBBBGGGGGRRRRR` (5 bits por canal, bit 15 ignorado). A entrada 0 de ambas as paletas é sempre transparente.

## 3. Modos de Vídeo

O GBA possui 6 modos de vídeo, selecionados pelos bits 0-2 do registrador `DISPCNT`.

| Modo | Tipo | Backgrounds | Sprites | Descrição |
| :--- | :--- | :--- | :--- | :--- |
| **0** | Tile-based | 4 Texto (BG0-BG3) | 128 | Backgrounds estáticos, sem rotação/escala. |
| **1** | Tile-based | 2 Texto (BG0-BG1), 1 Afim (BG2) | 128 | Combinação de backgrounds estáticos e um com rotação/escala. |
| **2** | Tile-based | 2 Afim (BG2-BG3) | 128 | Dois backgrounds com rotação/escala. |
| **3** | Bitmap | 1 Frame (16 bpp) | 64 | Framebuffer de 240x160, 16 bpp (32.768 cores). |
| **4** | Bitmap | 2 Frames (8 bpp) | 64 | Dois framebuffers de 240x160, 8 bpp (256 cores) - para page flipping. |
| **5** | Bitmap | 2 Frames (16 bpp) | 64 | Dois framebuffers de 160x128, 16 bpp - para page flipping. |

### 3.1. Modos Baseados em Tiles (Modos 0, 1, 2)

Nestes modos, a imagem é construída a partir de tiles (blocos de 8x8 pixels) organizados em mapas de tela. O PPU renderiza os tiles de acordo com os parâmetros configurados nos registradores de controle de cada background (`BGxCNT`).

- **Backgrounds de Texto:** Usam um mapa de tela de 32x32 tiles (2 KB). Cada entrada do mapa é um índice de 16 bits que especifica o tile a ser desenhado, a paleta a ser usada, e flags de espelhamento horizontal/vertical.
- **Backgrounds Afim:** Usam um mapa de tela de até 128x128 tiles. Cada entrada é um índice de 16 bits para o tile. A rotação e escala são controladas pelos registradores `BGxPA`, `BGxPB`, `BGxPC`, `BGxPD`, `BGxX` e `BGxY`.

### 3.2. Modos Bitmap (Modos 3, 4, 5)

Nestes modos, a imagem é renderizada diretamente em um framebuffer na VRAM. O PPU simplesmente exibe o conteúdo da VRAM como uma imagem.

- **Modo 3:** Um único framebuffer de 240x160, 16 bpp. O endereço base é `0x0600_0000`. O tamanho do framebuffer é 240 * 160 * 2 = 76.800 bytes.
- **Modo 4:** Dois framebuffers de 240x160, 8 bpp. O endereço base é `0x0600_0000` (Frame 0) e `0x0600_A000` (Frame 1). O tamanho de cada framebuffer é 240 * 160 = 38.400 bytes.
- **Modo 5:** Dois framebuffers de 160x128, 16 bpp. O endereço base é `0x0600_0000` (Frame 0) e `0x0600_A000` (Frame 1). O tamanho de cada framebuffer é 160 * 128 * 2 = 40.960 bytes.

## 4. Registradores de Controle de Vídeo

A tabela abaixo descreve os registradores mais importantes para o controle do subsistema de vídeo.

| Endereço | Nome | Bits | Descrição |
| :--- | :--- | :--- | :--- |
| `0x0400_0000` | `DISPCNT` | 0-2 | Modo de vídeo (0-5). |
| | | 3 | Bit de modo GBC (somente leitura). |
| | | 4 | Page flipping para modos bitmap (0 = Frame 0, 1 = Frame 1). |
| | | 5 | Processamento forçado durante HBlank (reduz flicker). |
| | | 6 | Organização de sprites (0 = 2D, 1 = 1D). |
| | | 7 | Força display em branco. |
| | | 8-11 | Ativação de backgrounds (BG0-BG3). |
| | | 12 | Ativação de sprites. |
| | | 13-15 | Ativação de janelas (Window 0, Window 1, Sprite Window). |
| `0x0400_0004` | `DISPSTAT` | 0 | Status de VBlank (0 = VDraw, 1 = VBlank). |
| | | 1 | Status de HBlank (0 = HDraw, 1 = HBlank). |
| | | 2 | Status de trigger de VCount. |
| | | 3 | Habilita IRQ de VBlank. |
| | | 4 | Habilita IRQ de HBlank. |
| | | 5 | Habilita IRQ de trigger de VCount. |
| | | 8-15 | Valor de trigger de VCount (linha de scan). |
| `0x0400_0006` | `VCOUNT` | 0-15 | Linha de scan atual (0-227). |
| `0x0400_0008` | `BG0CNT` | 0-1 | Prioridade do BG0 (0-3). |
| | | 2-3 | Endereço base dos tiles (charblock). |
| | | 6 | Ativação de mosaic. |
| | | 7 | Modo de paleta (0 = 16 paletas de 16 cores, 1 = 1 paleta de 256 cores). |
| | | 8-12 | Endereço base do mapa de tela (screenblock). |
| | | 13 | Screen Over (para backgrounds afim). |
| | | 14-15 | Tamanho do mapa de tela. |
| `0x0400_0010` | `BG0HOFS` | 0-9 | Scroll horizontal do BG0 (pixels). |
| `0x0400_0012` | `BG0VOFS` | 0-9 | Scroll vertical do BG0. |
| `0x0400_0020` | `BG2PA` | 0-15 | Parâmetro Afim (escala X). |
| `0x0400_0022` | `BG2PB` | 0-15 | Parâmetro Afim (shear X). |
| `0x0400_0024` | `BG2PC` | 0-15 | Parâmetro Afim (shear Y). |
| `0x0400_0026` | `BG2PD` | 0-15 | Parâmetro Afim (escala Y). |
| `0x0400_0028` | `BG2X` | 0-27 | Posição X do ponto de referência. |
| `0x0400_002C` | `BG2Y` | 0-27 | Posição Y do ponto de referência. |

### 4.1. Configuração de Cores

As cores são representadas como valores de 16 bits no formato:

```
Bit: 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
     X  B  B  B  B  B  G  G  G  G  G  R  R  R  R  R
```

Onde:
- **R** (bits 0-4): Valor de vermelho (0-31).
- **G** (bits 5-9): Valor de verde (0-31).
- **B** (bits 10-14): Valor de azul (0-31).
- **X** (bit 15): Ignorado.

### 4.2. Formato de Cores em Modos Paletizados

Em modos paletizados (4 bpp e 8 bpp), os pixels são índices para a Palette RAM. O PPU usa esses índices para buscar a cor de 16 bits.

- **4 bpp:** Cada pixel usa 4 bits (16 cores). Dois pixels por byte.
- **8 bpp:** Cada pixel usa 8 bits (256 cores). Um pixel por byte.

## 5. Pipeline de Renderização

O PPU do GBA renderiza a tela linha por linha, seguindo a ordem de prioridade. Para cada scanline, o PPU:

1.  **Processa os Backgrounds:** Renderiza os backgrounds ativos na ordem de prioridade (BG0 -> BG1 -> BG2 -> BG3).
2.  **Processa os Sprites:** Renderiza os sprites ativos, também na ordem de prioridade.
3.  **Aplica Efeitos:** Aplica os efeitos de windowing, mosaic e blending.
4.  **Saída:** O pixel final é enviado para o display.

### 5.1. Renderização de Backgrounds

- **Texto:** O PPU percorre o mapa de tela, lê o índice do tile, busca os pixels do tile na VRAM e aplica a paleta correta.
- **Afim:** O PPU usa as matrizes de transformação (PA, PB, PC, PD) para mapear pixels da VRAM para a tela. Permite rotação, escala e translação.

### 5.2. Renderização de Sprites

O PPU lê as entradas da OAM para cada sprite e renderiza os tiles correspondentes, aplicando atributos como posição, tamanho, prioridade, e transformações afim.

### 5.3. Efeitos Especiais

- **Mosaic:** Reduz a resolução de um background ou sprite, agrupando pixels em blocos.
- **Windowing:** Divide a tela em regiões com diferentes configurações de exibição.
- **Blending:** Combina pixels de diferentes camadas para criar transparência ou efeitos de fade. Os modos de blend são:
    - `01`: Alpha blend (transparência entre duas camadas).
    - `10`: Lighten (fade para branco).
    - `11`: Darken (fade para preto).

## 6. Implementação no Emulador

### 6.1. Modelagem da Memória de Vídeo

O emulador deve modelar as regiões de VRAM, OAM e Palette RAM como arrays de bytes. O acesso a essas regiões deve ser feito através de funções de leitura/escrita que respeitam as larguras de barramento (16-bit para VRAM e Palette RAM, 32-bit para OAM).

### 6.2. Pipeline de Renderização

O pipeline de renderização deve ser executado scanline por scanline. Para cada scanline, o emulador deve:

1.  **Ler os Registradores:** Ler os registradores de controle de vídeo (`DISPCNT`, `BGxCNT`, etc.).
2.  **Determinar o Modo:** Com base em `DISPCNT`, determinar o modo de vídeo.
3.  **Renderizar Backgrounds:** Renderizar os backgrounds de acordo com o modo e os parâmetros configurados.
4.  **Renderizar Sprites:** Renderizar os sprites de acordo com os atributos na OAM.
5.  **Aplicar Efeitos:** Aplicar os efeitos de windowing, mosaic e blending.
6.  **Gerar Pixel:** Gerar o pixel final e armazená-lo em um framebuffer interno.
7.  **Trigger de Interrupções:** No final de cada scanline (HBlank) e no final do quadro (VBlank), verificar se as interrupções correspondentes estão habilitadas e dispará-las.

### 6.3. Considerações de Desempenho

A renderização do PPU é uma das partes mais intensivas em processamento de um emulador de GBA. Para otimizar o desempenho:

- **Renderização em Lote:** Em vez de renderizar pixel por pixel, considere renderizar blocos de tiles ou sprites inteiros de uma só vez.
- **Cache de Tiles:** Mantenha tiles renderizados em cache para evitar a conversão repetida de dados de tile para pixels.
- **Otimizações por Modo:** Implemente rotinas de renderização específicas para cada modo de vídeo.

## 7. Conclusão

O subsistema de vídeo do GBA é poderoso e flexível, permitindo uma grande variedade de estilos gráficos. A compreensão detalhada de sua arquitetura, modos de operação e pipeline de renderização é essencial para a implementação precisa e eficiente do emulador `zGBA`. Este documento serve como guia completo para essa tarefa.