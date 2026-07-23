# O Mapa do Desenvolvimento do zGBA
## 1. v0.2.0 — Decodificador Completo da CPU (ARM7TDMI)
Atualmente a CPU só entende instruções ultra básicas. Para rodar código de verdade, falta implementar:

- Set de Instruções ARM: Operações de Data Processing (ADD, SUB, MOV, AND, ORR), instruções de desvio (B, BL) e acesso à memória (LDR, STR, LDRB, STRB).

- Set de Instruções THUMB: O modo de 16 bits usado pela esmagadora maioria dos jogos para economizar espaço de memória.

- Tratamento de Modos e Registradores: Manipular corretamente os registradores de cada modo do processador (Supervisor, IRQ, System) e ajustar as flags de status do registrador CPSR (Zero, Negative, Carry, Overflow).

## 2. v0.3.0 — PPU (Processador de Vídeo) e Modos de Renderização
O visualizador do GBA é baseado em registradores de I/O específicos:

- Modos Bitmap (3, 4 e 5):

    - Modo 3: Já temos a estrutura base (240x160 a 16 bits), falta refinar a paleta/conversão de cores.

    - Modo 4: Renderização por Paleta de Cores (8 bits por pixel + Paleta de 256 cores na RAM de Paleta).

- Modos Tile/Background (Modos 0, 1 e 2):

    - Motor de ladrilhos (Tiles de 8x8 pixels).

    - Gerenciamento das 4 camadas de fundo (Backgrounds BG0-BG3) com rolagem de tela (scrolling).

- Sprites (OBJ - Objects): Renderização de personagens e elementos móveis (VRAM de Sprites + OAM - Object Attribute Memory).

## 3. v0.4.0 — Interrupções (IRQ), Timers e Mapeamento de Teclado
Sem interrupções e temporizadores, os jogos não conseguem controlar a lógica do tempo.

- Timers (TM0-TM3): Contadores de tempo do hardware baseados na frequência de 16.78 MHz.

- Gerenciador de IRQ: Avisar a CPU quando um frame termina (V-Blank), quando a linha H-Blank é atingida ou quando um Timer estoura.

- Keypad (KEYINPUT): Mapear os botões do SDL (A, B, D-Pad, Start, Select, L, R) para o registrador 0x04000130 da memória do GBA.

## 4. v0.5.0 — Áudio (APU) e Save States
- Canais de Som: Implementar os 4 canais legados do Game Boy (Tone, Sweep, Noise) e os 2 canais de som direto (Direct Sound / DMA).

- Sistemas de Salvar (Backup Media): Suporte a SRAM, Flash Memory (512k/1M) e EEPROM (necessário para salvar o progresso nos jogos).