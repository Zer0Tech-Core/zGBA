# Changelog

Todas as mudanças notáveis no projeto **zGBA** serão documentadas neste arquivo.

O formato é baseado no [Keep a Changelog](https://keepachangelog.com/pt-BR/1.0.0/) e este projeto adere ao [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [0.5.0] - Phase 5 Complete - 2026-07-21

### Adicionado
- **Integração do Loop com HALT/IRQ:** Loop principal ajustado para avançar os relógios de PPU, Timer e DMA mesmo quando a CPU entra em `HALT` (SWI 0x02).
- **Cartridge & Save States:** Suporte inicial para persistência de dados e barramento de SRAM/Flash.
- **APU (Audio Processing Unit):** Estrutura inicial da APU acoplada ao barramento de memória com suporte a registros FIFO e PSG.
- **Suporte a ROMs Comerciais:** Carregamento e execução bem-sucedidos de *Pokémon FireRed* executando mais de 100M de instruções sem travamento.

### Corrigido
- Loop do emulador travando quando a CPU recebia instrução de `HALT`.
- Remoção da escrita forçada manual no registrador `DISPCNT` (`0x04000000`), deixando a configuração de exibição sob responsabilidade da própria ROM/BIOS.

---

## [0.4.0] - Phase 4: Subsystems Integration

### Adicionado
- **DMA (Direct Memory Access):** Implementação dos 4 canais de DMA com suporte a transferência de memória via VBlank e HBlank.
- **Timers:** Implementação dos 4 Timers do GBA com suporte a prescalers e estouro de contador.
- **Pipeline PPU/Display:** Renderização de frames via buffer usando SDL2 em tempo real.

---

## [0.3.0] - Phase 3: PPU & Basic Rendering

### Adicionado
- **PPU (Pixel Processing Unit):** Implementação preliminar do Modo 0 e suporte a mapas de tiles/backgrounds.
- **Display SDL2:** Janela nativa em C++ para exibição do buffer de renderização do GBA (240x160).

---

## [0.2.0] - Phase 2: Memory & Bus Architecture

### Adicionado
- **Barramento de Memória Completo:** Mapeamento de EWRAM, IWRAM, VRAM, OAM, Palette RAM, IO Registers e ROM Game Pak.
- **BIOS HLE:** Fallback para High-Level Emulation de chamadas de sistema (SWI) quando nenhuma BIOS física é fornecida.

---

## [0.1.0] - Phase 1: Core CPU (ARM7TDMI)

### Adicionado
- **Decoder ARM & Thumb:** Suporte às rotinas de instrução ARM (32-bit) e Thumb (16-bit).
- **Registradores e Pipeline:** Implementação do banco de registradores (R0-R15, CPSR, SPSR) e troca de modos de execução.

---

## [0.0.1] - Initial Setup

### Adicionado
- Estrutura inicial do projeto em C++ com CMake.
- Gerador de ROMs de teste em Python para validação de instruções básicas.