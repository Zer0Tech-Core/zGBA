# zGBA Architecture & Implementation Roadmap

Este documento serve como mapa de desenvolvimento e especificação técnica para o emulador **zGBA**.

---

## 1. Visão Geral da Arquitetura do Hardware

O Game Boy Advance é um sistema baseado no processador ARM7TDMI operando em uma arquitetura de 32 bits, com barramento de memória unificado e componentes dedicados para PPU (Graphics), APU (Sound), DMA, Timers e Interrupções.


```
   +-------------------------------------------------------+
   |                  ARM7TDMI CPU (16.78 MHz)              |
   |               (Executa modos ARM e THUMB)              |
   +---------------------------+---------------------------+
                               |
                               v
   +-------------------------------------------------------+
   |                  Sistema de Memória                   |
   | - System ROM / BIOS (16KB)                            |
   | - EWRAM / Board (256KB)  | IWRAM / Chip (32KB)        |
   | - IO Registers (1KB)                                  |
   | - Palette RAM (1KB)      | VRAM (96KB) | OAM (1KB)    |
   | - Game Pak / ROM (Até 32MB)                           |
   +---+-----------------------+-----------------------+---+
       |                       |                       |
       v                       v                       v
+--------------+        +--------------+        +--------------+
| PPU Display  |        | DMA & Timers |        |  APU Sound   |
|  (240x160)   |        |  (4 Canais)  |        | (PSG + Direct|
| Modos 0-5    |        | (Timers 0-3) |        |    Sound)    |
+--------------+        +--------------+        +--------------+

```

---

## 2. Componentes Principais & Especificações Técnicas

### 2.1 CPU: ARM7TDMI (32-bit RISC)
* **Frequency:** 16.777216 MHz (~16.78 MHz).
* **Modes:** User, FIQ, IRQ, Supervisor (SVC), Abort, Undefined, System.
* **Instruction Sets:**
  * **ARM State (32-bit):** Instruções alinhadas a 4 bytes.
  * **THUMB State (16-bit):** Instruções alinhadas a 2 bytes (maior densidade de código).
* **Pipeline:** 3 estágios (`Fetch` -> `Decode` -> `Execute`).
  * *Comportamento PC (r15):* Em estado ARM, `PC` lê o endereço da instrução atual + 8. Em estado THUMB, `PC` lê o endereço atual + 4.

### 2.2 Mapa de Memória (Memory Map)

| Endereço Inicial | Endereço Final | Tamanho | Região / Propósito |
|---|---|---|---|
| `0x00000000` | `0x00003FFF` | 16 KB | BIOS (System ROM) |
| `0x02000000` | `0x0203FFFF` | 256 KB | EWRAM (On-board External WRAM) |
| `0x03000000` | `0x03007FFF` | 32 KB | IWRAM (On-chip Internal WRAM) |
| `0x04000000` | `0x040003FF` | 1 KB | IO Registers |
| `0x05000000` | `0x050003FF` | 1 KB | Palette RAM (512 B BG / 512 B OBJ) |
| `0x06000000` | `0x06017FFF` | 96 KB | VRAM (Video RAM) |
| `0x07000000` | `0x070003FF` | 1 KB | OAM (Object Attribute Memory - Sprites) |
| `0x08000000` | `0x09FFFFFF` | Até 32 MB | Game Pak ROM (Waitstate 0) |
| `0x0A000000` | `0x0BFFFFFF` | Até 32 MB | Game Pak ROM (Waitstate 1) |
| `0x0C000000` | `0x0DFFFFFF` | Até 32 MB | Game Pak ROM (Waitstate 2) |
| `0x0E000000` | `0x0E00FFFF` | Até 64 KB | Game Pak SRAM / Flash Memory (Save Data) |

### 2.3 Display e PPU (Picture Processing Unit)
* **Resolução Real:** 240 x 160 pixels.
* **Taxa de Atualização:** 59.727 Hz.
* **Timings do Scanline:**
  * 1 frame = 228 scanlines (Lines 0-159: Visible; Lines 160-227: VBlank).
  * 1 scanline = 1232 ciclos de clock da CPU (960 HDraw + 272 HBlank).
* **Modos de Vídeo (`DISPCNT` - `0x04000000`):**
  * **Modo 0:** Tile-based (BG0, BG1, BG2, BG3 - todos text/tile).
  * **Modo 1:** Tile-based (BG0, BG1 Text + BG2 Affine/RotScale).
  * **Modo 2:** Tile-based (BG2, BG3 Affine/RotScale).
  * **Modo 3:** Bitmap Modo Direct Color (240x160 @ 16-bit BGR555, Framebuffer único).
  * **Modo 4:** Bitmap Modo Indexed Color (240x160 @ 8-bit com Palette, Dual-framebuffer).
  * **Modo 5:** Bitmap Modo Direct Color Reduzido (160x128 @ 16-bit, Dual-framebuffer).

### 2.4 Registradores I/O Essenciais
* **Display/LCD:**
  * `DISPCNT` (`0x04000000`): Controle principal de vídeo (Modo, visibilidade de BGs/OBJ).
  * `DISPSTAT` (`0x04000004`): Status de VBlank, HBlank, VCounter Trigger.
  * `VCOUNT` (`0x04000006`): Scanline atual (0-227).
* **Interrupções:**
  * `IE` (`0x04000200`): Interrupt Enable (VBlank, HBlank, VCounter, Timers, DMA, etc.).
  * `IF` (`0x04000202`): Interrupt Flags (Acknowledge / Pendentes).
  * `IME` (`0x04000208`): Interrupt Master Enable (1 = Habilita interrupções globais).

---

## 3. Roadmap de Implementação para o zGBA

---

### Fase 1: Núcleo CPU & Decodificador Completo (ARM + THUMB)
- [ ] **ARM Instruction Set completo:**
  - Complete Data Processing (ADC, SBC, RSC, TST, TEQ, CMP, CMN, ORR, BIC, MVN).
  - Multiplicação avançada (UMULL, UMLAL, SMULL, SMLAL).
  - Single Data Swap (SWP/SWPB).
- [ ] **THUMB Instruction Set completo:**
  - Formatos 1 a 19 sem omissão de casos de borda.
  - Ajuste de suporte para shifts por registrador.
- [ ] **Pipeline & Execução Ciclo-a-Ciclo:**
  - Ajustar o avanço do `r[15]` para simular rigorosamente o pipeline de 3 estágios (`PC+8` para ARM, `PC+4` para THUMB).
  - Implementar gerenciamento estrito do `CPSR` e troca banked de registradores ao mudar de modos de CPU (FIQ, IRQ, SVC).

---

### Fase 2: PPU Pixel Pipeline por Scanline (HDraw / HBlank / VBlank)
- [ ] **Renderização Scanline-by-Scanline:**
  - Mudar o renderizador do zGBA de renderização por frame (framebuffer) para scanline.
  - Atualizar `VCOUNT` (`0x04000006`) a cada 1232 ciclos do processador.
- [ ] **Modos de Vídeo Tile Engine (Modos 0, 1 e 2):**
  - Implementar renderização de BGs Text (32x32, 64x64 tiles, 4bpp e 8bpp).
  - Implementar suporte completo a **Affine Backgrounds** (Rotação e Escala via matrizes nos registradores `BG2PA-BG2PD`).
- [ ] **Sprites e OAM Engine:**
  - Renderização de Sprites (4bpp / 8bpp, tamanhos de 8x8 até 64x64 pixels).
  - Sprites com transformações Affine (Rotação/Escala).
  - Transparência de Sprite, Colisão de Janela (Windowing) e Prioridades de Camadas (Layers 0-3).

---

### Fase 3: DMA (Direct Memory Access) & Timers
- [ ] **DMA Engine (4 Canais: DMA0 - DMA3):**
  - **DMA0:** Maior prioridade (Apenas memória interna/rápida).
  - **DMA1/DMA2:** Áudio / Direct Sound FIFO.
  - **DMA3:** Geral / Transferência para Game Pak e VRAM.
  - Modos de Disparo: Immediate, VBlank, HBlank, Special/Audio.
- [ ] **Timers Engine (4 Timers: TM0 - TM3):**
  - Contadores de 16-bit com Prescalers (1, 64, 256, 1024 ciclos).
  - Suporte a Cascade Mode (Timer incrementa quando o anterior estoura).
  - Disparo de interrupções IRQ no overflow.

---

### Fase 4: Gerenciamento Preciso de Interrupções & BIOS
- [ ] **Interrupt Controller Integro:**
  - Processamento estrito de `IE` (`0x04000200`), `IF` (`0x04000202`) e `IME` (`0x04000208`).
  - Chamada real do vetor de interrupções em `0x00000018` quando o flag `FLAG_I` do `CPSR` permite.
- [ ] **Expansão SWI / BIOS HLE:**
  - Suporte a chamadas do BIOS SWI: `CpuSet`, `CpuFastSet`, `LZ77UncompReadBy8`, `SquareRoot`, `ArcTan2`, `Div`.

---

### Fase 5: APU Áudio & Save States (Cartridge Backup)
- [ ] **Canais PSG (Game Boy Classic Legacy Sound):**
  - Canal 1 & 2: Onda Quadrada com Sweep e Envelope.
  - Canal 3: Programmable Waveform.
  - Canal 4: Gerador de Ruído (Noise).
- [ ] **Direct Sound (Canais A e B):**
  - Buffers FIFO de 8 bits alinhados com DMA1/DMA2 e Timers.
  - Mixagem de áudio em buffer estéreo PCM (ex: via SDL_Audio).
- [ ] **Cartridge Save Types:**
  - Detecção automática e suporte a salvar arquivos `.sav`:
    - SRAM (32KB).
    - EEPROM (4KB / 64KB).
    - Flash Memory (64KB / 128KB).
