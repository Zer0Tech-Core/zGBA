# Documentação Técnica do Game Boy Advance (GBA) para o Emulador zGBA

Este documento contém as principais especificações de hardware e programação do GBA, extraídas do arquivo `GBATEK.txt`. O foco está nas informações necessárias para a implementação de um emulador preciso.

## 1. Visão Geral do Hardware

### 1.1. CPU e Modos de Operação

*   **CPU:** ARM7TDMI (32-bit RISC).
*   **Modos:**
    *   **ARM Mode:** Instruções de 32 bits. Clock de 16.78 MHz.
    *   **THUMB Mode:** Instruções de 16 bits. Clock de 16.78 MHz (recomendado para código em ROM, por ser mais eficiente).
*   **Modos de Compatibilidade (não suportados por todos os jogos e não são o foco principal):**
    *   **CGB Mode:** Modo de compatibilidade com Game Boy Color.
    *   **DMG Mode:** Modo de compatibilidade com Game Boy original.

### 1.2. Memória Interna

| Região         | Tamanho       | Descrição                                      | Notas sobre Acesso                          |
|----------------|---------------|------------------------------------------------|---------------------------------------------|
| `00000000`     | 16 KB         | BIOS ROM                                       | Protegida contra leitura (retorna opcode pré-buscado se PC não estiver na BIOS). |
| `02000000`     | 256 KB        | WRAM - Work RAM (On-board, lenta)              | Acesso: 8/16/32 bits, com waitstates.      |
| `03000000`     | 32 KB         | WRAM - Work RAM (On-chip, rápida)              | Acesso: 8/16/32 bits, 1 ciclo.             |
| `04000000`     | 1 KB          | I/O Registers                                  | Mapeamento de hardware.                     |
| `05000000`     | 1 KB          | Palette RAM (BG e OBJ)                         | Acesso permitido apenas durante H-Blank ou V-Blank (ou com Forced Blank ativo). |
| `06000000`     | 96 KB         | VRAM - Video RAM (dividido em 64KB BG e 32KB OBJ) | Acesso permitido apenas durante H-Blank ou V-Blank (ou com Forced Blank ativo). |
| `07000000`     | 1 KB          | OAM - Object Attribute Memory                  | Acesso permitido apenas durante H-Blank ou V-Blank (ou com Forced Blank ativo). |
| `08000000`     | até 32 MB     | Game Pak ROM (Wait State 0)                    | Bus de 16 bits. Leitura de 32 bits requer dois acessos. |
| `0A000000`     | até 32 MB     | Game Pak ROM (Wait State 1)                    |                                            |
| `0C000000`     | até 32 MB     | Game Pak ROM (Wait State 2)                    |                                            |
| `0E000000`     | até 64 KB     | Game Pak SRAM/Flash                            | Bus de 8 bits. Apenas acesso por byte.     |

**Observações Importantes:**

*   **VRAM/OAM/Palette:** O acesso da CPU a essas áreas pode causar um waitstate se o controlador de vídeo estiver acessando a memória simultaneamente. O modo "Forced Blank" (DISPCNT Bit 7) permite acesso irrestrito.
*   **WRAM (02000000h):** Pode ser "overclockada" via um registro não documentado (`4000800h`), mas isso não é suportado no DS e pode travar o GBA Micro.
*   **GamePak ROM:** É recomendado usar código THUMB na ROM, pois instruções ARM (32 bits) exigem dois acessos de 16 bits ao barramento, reduzindo a performance.

### 1.3. Vídeo

*   **Display:** 240x160 pixels, 2.9 polegadas (TFT LCD). O GBA SP tem backlight; o original não.
*   **Camadas (Layers):** 4 camadas de fundo (BG0 a BG3).
*   **Tipos de BG:**
    *   **Tile/Map-based (Modos 0, 1, 2):** O fundo é construído a partir de tiles (8x8 pixels) organizados em um mapa.
    *   **Bitmap-based (Modos 3, 4, 5):** O fundo é uma imagem bitmap direta.
*   **Cores:**
    *   **Tile modes:** 256 cores (1 palette) ou 16 cores com 16 paletas (256 cores no total).
    *   **Bitmap modes:** 32768 cores (15-bit RGB).
*   **Sprites (OBJs):**
    *   Máximo de 128 sprites por tela.
    *   Tamanhos variados (8x8 até 64x64).
    *   Suporte a rotação e escala, mosaico, semi-transparência, etc.
*   **Efeitos Especiais:** Rotação/Escala, Alpha Blending, Fade-in/out, Mosaico, Janelas (Windows).

### 1.4. Som

*   **Canais:**
    *   4 canais analógicos (compatíveis com CGB): 3 ondas quadradas, 1 ruído.
    *   2 canais digitais (DMA Sound A e B): para reprodução de amostras PCM 8-bit.
*   **Saída:** Alto-falante mono (embutido) e saída estéreo para fones de ouvido.
*   **FIFO:** Cada canal DMA possui um FIFO de 32 bytes (8 x 32 bits).

### 1.5. Controles e Comunicação

*   **Gamepad:** 4 direcionais + 6 botões (A, B, L, R, Start, Select).
*   **Porta Serial (SIO):** Para comunicação com outros GBAs (link cable) ou periféricos (ex: Wireless Adapter). O Nintendo DS não possui essa porta.
*   **Modos SIO:** Normal (8/32 bits), Multi-Player (até 4 GBAs), UART, JOY BUS, General Purpose (I/O paralelo).

### 1.6. Temporização

*   **Clock do Sistema:** 16.78 MHz (16.78 * 1024 * 1024 Hz). Um ciclo = ~59.59 ns.
*   **Timing Horizontal:**
    *   240 pixels visíveis (960 ciclos).
    *   68 pixels de H-Blank (272 ciclos).
    *   Total: 308 pixels (1232 ciclos) ~ 13.620 kHz.
*   **Timing Vertical:**
    *   160 linhas visíveis (197,120 ciclos).
    *   68 linhas de V-Blank (83,776 ciclos).
    *   Total: 228 linhas (280,896 ciclos) ~ 59.737 Hz.

## 2. Registros I/O Importantes

Esta seção lista os registros de I/O mais cruciais para a emulação. A lista completa está no `GBATEK.txt`.

### 2.1. Controle de Vídeo (LCD)

*   **`4000000h` - DISPCNT (LCD Control)**
    *   Bits 0-2: Modo de BG (0-5).
    *   Bit 4: Seleção de Frame (para Modos 4 e 5).
    *   Bit 5: H-Blank Interval Free (permite acesso à OAM durante H-Blank).
    *   Bit 6: Mapeamento de VRAM de OBJ (0=2D, 1=1D).
    *   Bit 7: Forced Blank (permite acesso livre à VRAM/Palette/OAM).
    *   Bits 8-11: Enable para BG0, BG1, BG2, BG3.
    *   Bit 12: Enable para OBJ.
    *   Bits 13-15: Enable para Janela 0, Janela 1, Janela OBJ.
*   **`4000004h` - DISPSTAT (General LCD Status)**
    *   Bit 0: V-Blank Flag (R).
    *   Bit 1: H-Blank Flag (R).
    *   Bit 2: V-Counter Flag (R) (quando VCOUNT == LYC).
    *   Bit 3: V-Blank IRQ Enable.
    *   Bit 4: H-Blank IRQ Enable.
    *   Bit 5: V-Counter IRQ Enable.
    *   Bits 8-15: V-Count Setting (LYC).
*   **`4000006h` - VCOUNT (Vertical Counter) (R)**
    *   Bits 0-7: Linha atual (0-227).
*   **`4000008h` a `400000Eh` - BG0CNT a BG3CNT (BG Control)**
    *   Bits 0-1: Prioridade do BG (0=mais alta).
    *   Bits 2-3: Base do bloco de caracteres (Tile Data).
    *   Bit 6: Enable Mosaico.
    *   Bit 7: Modo de Cores (0=16/16, 1=256/1).
    *   Bits 8-12: Base do bloco de tela (Map Data).
    *   Bit 13: BG2/BG3: Area Overflow (0=Transparente, 1=Wraparound).
    *   Bits 14-15: Tamanho da Tela.
*   **`4000010h` a `400001Eh` - BG0HOFS a BG3VOFS (BG Scrolling)**
    *   Bits 0-8: Offset X/Y (0-511). Usado em Modos de Texto.
*   **`4000028h` - BG2X / `400002Ch` - BG2Y (BG Reference Point)**
    *   Usado em Modos de Rotação/Escala e Bitmap. Especifica a posição do pixel no mapa/bitmap que será exibido no canto superior esquerdo.
*   **`4000040h` - WIN0H / `4000042h` - WIN1H (Window Horizontal Dimensions)**
    *   Bits 0-7: X2 (coordenada direita +1).
    *   Bits 8-15: X1 (coordenada esquerda).
*   **`4000044h` - WIN0V / `4000046h` - WIN1V (Window Vertical Dimensions)**
    *   Bits 0-7: Y2 (coordenada inferior +1).
    *   Bits 8-15: Y1 (coordenada superior).
*   **`4000048h` - WININ (Control Inside of Window(s))**
*   **`400004Ah` - WINOUT (Control Outside of Windows & Inside of OBJ Window)**
*   **`400004Ch` - MOSAIC (Mosaic Size)**
*   **`4000050h` - BLDCNT (Color Special Effects Selection)**
*   **`4000052h` - BLDALPHA (Alpha Blending Coefficients)**
*   **`4000054h` - BLDY (Brightness Coefficient)**

### 2.2. Controle de Som

*   **`4000060h` - SOUND1CNT_L (Channel 1 Sweep)**
*   **`4000062h` - SOUND1CNT_H (Channel 1 Duty/Length/Envelope)**
*   **`4000064h` - SOUND1CNT_X (Channel 1 Frequency/Control)**
*   **`4000068h` - SOUND2CNT_L (Channel 2 Duty/Length/Envelope)**
*   **`400006Ch` - SOUND2CNT_H (Channel 2 Frequency/Control)**
*   **`4000070h` - SOUND3CNT_L (Channel 3 Stop/Wave RAM select)**
*   **`4000072h` - SOUND3CNT_H (Channel 3 Length/Volume)**
*   **`4000074h` - SOUND3CNT_X (Channel 3 Frequency/Control)**
*   **`4000078h` - SOUND4CNT_L (Channel 4 Length/Envelope)**
*   **`400007Ch` - SOUND4CNT_H (Channel 4 Frequency/Control)**
*   **`4000080h` - SOUNDCNT_L (Stereo/Volume/Enable)**
*   **`4000082h` - SOUNDCNT_H (Mixing/DMA Control)**
*   **`4000084h` - SOUNDCNT_X (Sound on/off)**
*   **`4000088h` - SOUNDBIAS (Sound PWM Control)**
*   **`4000090h` - WAVE_RAM (Channel 3 Wave Pattern RAM)**
*   **`40000A0h` - FIFO_A (Channel A FIFO)**
*   **`40000A4h` - FIFO_B (Channel B FIFO)**

### 2.3. DMA

*   **`40000B0h` - DMA0SAD (Source Address)**
*   **`40000B4h` - DMA0DAD (Destination Address)**
*   **`40000B8h` - DMA0CNT_L (Word Count)**
*   **`40000BAh` - DMA0CNT_H (Control)**
    *   Bits 5-6: Controle de endereço de destino (Increment, Decrement, Fixed).
    *   Bits 7-8: Controle de endereço de origem.
    *   Bit 9: Repeat.
    *   Bit 10: Transfer Type (16-bit ou 32-bit).
    *   Bit 11: Game Pak DRQ (DMA3 apenas).
    *   Bits 12-13: Start Timing (Immediate, VBlank, HBlank, Special).
    *   Bit 14: IRQ upon end.
    *   Bit 15: Enable.
*   **`40000BCh` a `40000DEh`:** Registros para DMA1, DMA2 e DMA3.

### 2.4. Timers

*   **`4000100h` - TM0CNT_L (Timer 0 Counter/Reload)**
*   **`4000102h` - TM0CNT_H (Timer 0 Control)**
    *   Bits 0-1: Prescaler (1, 64, 256, 1024).
    *   Bit 2: Count-up Timing (incrementa no overflow do timer anterior).
    *   Bit 6: IRQ Enable.
    *   Bit 7: Start/Stop.

### 2.5. Entrada (Keypad)

*   **`4000130h` - KEYINPUT (Key Status) (R)**
    *   Bits 0-9: Estado dos botões (0=Pressionado, 1=Liberado).
*   **`4000132h` - KEYCNT (Key Interrupt Control)**
    *   Bits 0-9: Seleciona quais botões geram interrupção.
    *   Bit 14: Enable.
    *   Bit 15: Condição (OR ou AND).

### 2.6. Interrupções

*   **`4000200h` - IE (Interrupt Enable)**
*   **`4000202h` - IF (Interrupt Request Flags / Acknowledge)**
*   **`4000208h` - IME (Interrupt Master Enable)**

### 2.7. Controle do Sistema

*   **`4000204h` - WAITCNT (Waitstate Control)**
    *   Bits 0-1: SRAM Wait Control.
    *   Bits 2-3, 5-6, 8-9: Wait State 0/1/2 First Access.
    *   Bits 4, 7, 10: Wait State 0/1/2 Second Access.
    *   Bits 11-12: PHI Terminal Output.
    *   Bit 14: Game Pak Prefetch Buffer Enable.
    *   Bit 15: Game Pak Type Flag (R).
*   **`4000300h` - POSTFLG (Post Boot Flag)**
*   **`4000301h` - HALTCNT (Low Power Mode Control)**
    *   Bit 7: 0=Halt, 1=Stop.
*   **`4000800h` - Undocumented - Internal Memory Control (R/W)**
    *   Bit 0: Disable 32K+256K WRAM (ou swap com 00000000h).
    *   Bit 3: Disable CGB Bootrom.
    *   Bit 5: Enable 256K WRAM (se não, espelha 32K WRAM).
    *   Bits 24-27: Wait Control para 256K WRAM.

## 3. Memória de Vídeo (VRAM)

### 3.1. Organização

*   **Modos 0, 1, 2 (Tile/Map):**
    *   `06000000h - 0600FFFFh` (64 KB): Compartilhado para BG Map e Tiles.
    *   `06010000h - 06017FFFh` (32 KB): Tiles de OBJ.
*   **Modo 3 (Bitmap 240x160, 32768 cores):**
    *   `06000000h - 06013FFFh` (80 KB): Frame 0 (apenas 75 KB usados).
    *   `06014000h - 06017FFFh` (16 KB): Tiles de OBJ.
*   **Modos 4 e 5 (Bitmap com 2 frames):**
    *   `06000000h - 06009FFFh` (40 KB): Frame 0.
    *   `0600A000h - 06013FFFh` (40 KB): Frame 1.
    *   `06014000h - 06017FFFh` (16 KB): Tiles de OBJ.

### 3.2. Dados de Tile (Character Data)

*   **4-bit (16 cores, 16 paletas):** 32 bytes por tile. Cada byte representa dois pixels (lower 4 bits = pixel esquerdo, upper 4 bits = pixel direito).
*   **8-bit (256 cores, 1 palette):** 64 bytes por tile. Cada byte é um índice de cor.

### 3.3. Mapa de Fundo (BG Map)

*   **Texto (Modos 0, 1):** 2 bytes por entrada.
    *   Bits 0-9: Número do Tile.
    *   Bit 10: Horizontal Flip.
    *   Bit 11: Vertical Flip.
    *   Bits 12-15: Número da Paleta.
*   **Rotação/Escala (Modos 1, 2):** 1 byte por entrada.
    *   Bits 0-7: Número do Tile.

## 4. Sprites (OBJs)

### 4.1. Atributos OAM (6 bytes por sprite)

*   **Atributo 0:**
    *   Bits 0-7: Y-Coordinate.
    *   Bit 8: Rotation/Scaling Flag.
    *   Bit 9: (Se Rotation/Scaling) Double-Size Flag; (Se não) OBJ Disable.
    *   Bits 10-11: OBJ Mode (0=Normal, 1=Semi-Transparent, 2=OBJ Window).
    *   Bit 12: OBJ Mosaic.
    *   Bit 13: Colors/Palettes (0=16/16, 1=256/1).
    *   Bits 14-15: OBJ Shape (Square, Horizontal, Vertical).
*   **Atributo 1:**
    *   Bits 0-8: X-Coordinate.
    *   Bits 9-13: (Se Rotation/Scaling) Rotation/Scaling Parameter Selection; (Se não) Bits não usados.
    *   Bits 12-13: (Se não Rotation/Scaling) Horizontal/Vertical Flip.
    *   Bits 14-15: OBJ Size (depende da Shape).
*   **Atributo 2:**
    *   Bits 0-9: Character Name (Tile Number).
    *   Bits 10-11: Priority (0=mais alta).
    *   Bits 12-15: Palette Number (não usado em 256/1).

### 4.2. Mapeamento de Tiles

*   **2D (DISPCNT Bit 6 = 0):** Os tiles são organizados em uma matriz de 32x32 tiles.
*   **1D (DISPCNT Bit 6 = 1):** Os tiles são sequenciais, um após o outro.

## 5. DMA (Direct Memory Access)

*   **Canais:** DMA0 (maior prioridade), DMA1, DMA2, DMA3.
*   **Modos de Início (Start Timing):**
    *   0: Imediato.
    *   1: V-Blank.
    *   2: H-Blank.
    *   3: Especial (Sound FIFO para DMA1/2, Video Capture para DMA3).
*   **Repeat:** Se ativado, o DMA reinicia automaticamente.
*   **Som DMA:** DMA1 e DMA2 podem alimentar os FIFOs de som. Usam o modo "Special" com Repeat ativado.
*   **Game Pak DMA:** Apenas DMA3 pode acessar a ROM do cartucho.

## 6. Som

*   **Canais 1-4 (PSG):** Controlados pelos registros `SOUNDxCNT`. O canal 3 usa uma Wave RAM (`WAVE_RAM`) para formas de onda.
*   **Canais A e B (DMA Sound):** Recebem dados de amostras via DMA para os FIFOs (`FIFO_A`, `FIFO_B`).
*   **Frequência de Amostragem:** O hardware reamostra para 32.768 kHz por padrão (configurável via `SOUNDBIAS`).
*   **Volume Master:** Controlado por `SOUNDCNT_L`.

## 7. Interrupções

*   **V-Blank, H-Blank, V-Counter Match:** Interrupções de vídeo.
*   **Timer Overflows:** Timer0 a Timer3.
*   **DMA:** Conclusão de transferência DMA.
*   **Keypad:** Interrupção do teclado.
*   **Serial:** Interrupção da porta serial.
*   **Cartucho:** Interrupção externa do cartucho.

## 8. Temporização e Ciclos

*   **Waitstates:** O GBA usa waitstates para acessar memórias mais lentas, como a ROM do cartucho e a WRAM de 256KB. O registro `WAITCNT` configura esses tempos.
*   **Acesso a VRAM/OAM/Palette:** Acessos da CPU são atrasados se o controlador de vídeo estiver usando o barramento.
*   **Prefetch:** O GBA tem um buffer de prefetch para a ROM do cartucho, que pode acelerar a execução de código.
*   **Timing de Acesso à Memória:** A tabela no `GBATEK.txt` detalha os ciclos de clock para diferentes tipos de acesso e regiões de memória.

## 9. Considerações para o Emulador

*   **Precisão de Temporização:** A emulação precisa dos timings (especialmente H-Blank, V-Blank e DMA) é crucial para a compatibilidade.
*   **Modos de Vídeo:** Todos os 6 modos de BG precisam ser suportados, incluindo modos de tile, bitmap, rotação/escala e janelas.
*   **Efeitos Especiais:** Alpha blending, fade, mosaico e janelas devem ser implementados.
*   **Som:** A emulação dos 4 canais PSG e dos 2 canais DMA, incluindo o FIFO e a reamostragem, é necessária.
*   **BIOS:** O emulador deve poder executar o código da BIOS, ou alternativamente, implementar suas funções (como o tratamento de interrupções).
*   **Controles de Acesso à Memória:** A proteção da BIOS e as restrições de acesso a VRAM/OAM/Palette devem ser respeitadas.
*   **Testes:** Use ROMs de teste (como os da série "Homebrew") para verificar a precisão da emulação de CPU, vídeo e som.

