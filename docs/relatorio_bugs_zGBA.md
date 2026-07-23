# Relatório de Diagnóstico — zGBA (Bug de Tela Preta)

**Data:** 22/07/2026
**Escopo analisado:** `platform/` (emulador, renderer, input, audio_backend, rom_loader, debugger), `core/memory/` (bus, memory_map), `core/audio/` (apu, mixer, channels), `core/video/` (ppu.hpp, video_types.hpp — **`ppu.cpp` ainda não analisado**), `core/cpu/` completo (ARM7TDMI, decoders ARM e Thumb, pipeline, registers, exceptions).

---

## Resumo Executivo

Foram encontrados **2 bugs críticos** que, isoladamente, já são suficientes para causar tela preta total, e **4 bugs secundários** que vão gerar problemas assim que os críticos forem corrigidos (travamentos em jogos comerciais, timing incorreto, input fantasma). A causa raiz mais provável e mais severa é a **nº 1** abaixo: nenhuma instrução ARM é executada no emulador.

| # | Severidade | Arquivo | Problema |
|---|---|---|---|
| 1 | 🔴 Crítico | `core/cpu/cpu.cpp` | `step_arm()` nunca decodifica/executa instruções ARM |
| 2 | 🔴 Crítico | `platform/emulador.cpp` | Loop de frame duplicado e desalinhado (`step()` já roda um frame inteiro, mas é chamado ~35 mil vezes por iteração externa) |
| 3 | 🟠 Alto | `core/memory/bus.cpp` | Registrador `IF` (0x04000202) sem semântica *write-1-to-clear* |
| 4 | 🟡 Médio | `core/memory/bus.cpp` | `request_interrupt()` conta ciclos de wait-state indevidamente |
| 5 | 🟡 Médio | `platform/emulador.cpp` / `platform/input.cpp` | `KEYINPUT` (0x04000130) nunca é escrito na memória mapeada |
| 6 | 🟢 Baixo | `core/cpu/arm/arm_load_store.hpp` | Offset de registrador em LDR/STR não trata ASR/ROR nem shift de 32 |
| 7 | 🟢 Baixo | `core/audio/apu.cpp` | Master enable (`soundcnt_x` bit 7) não é checado antes de mixar |

---

## 1. 🔴 CRÍTICO — CPU nunca executa instruções em modo ARM

**Arquivo:** `src/core/cpu/cpu.cpp`
**Função:** `CPU::step_arm()`

### O que está errado

```cpp
void CPU::step_arm() {
    PipelineStage stage = m_pipeline.step(m_regs, m_bus.read32, m_bus.read16);

    if (stage.valid) {
        bool needs_flush = false;   // <- hardcoded, nunca chama o decoder

        if (needs_flush) {
            m_pipeline.flush(m_regs, m_bus.read32, m_bus.read16);
        }
    }
}
```

`Pipeline::step()` apenas avança os estágios de *fetch* — ele não decodifica nem executa nada. A chamada a `ARM::ARMDecoder::decode_and_execute(...)` simplesmente não existe nesse método. Compare com `step_thumb()`, que faz isso corretamente.

### Por que isso causa tela preta

O CPSR pós-reset (`0x000000D3`) coloca a CPU em modo Supervisor com **T=0 (ARM)**. Toda a inicialização feita pela BIOS roda em ARM. Como `step_arm()` nunca decodifica nada, a CPU "anda" pelo espaço de memória sem que nenhuma instrução tenha efeito real: nenhum registrador é escrito, nenhum branch acontece, nenhuma configuração de vídeo (`DISPCNT`), paleta ou VRAM é tocada. Não é um travamento — é a CPU girando em vazio, silenciosamente, para sempre.

Isso também explica por que os testes unitários passam: eles provavelmente testam os decoders isoladamente (chamando `ARMDecoder::decode_and_execute` diretamente), não o fluxo real via `CPU::step()`.

### Como resolver

```cpp
#include "src/core/cpu/arm/arm_decoder.hpp"

void CPU::step_arm() {
    PipelineStage stage = m_pipeline.step(m_regs, m_bus.read32, m_bus.read16);

    if (stage.valid) {
        bool needs_flush = ARM::ARMDecoder::decode_and_execute(
            stage.instruction,
            m_regs,
            m_bus.read8, m_bus.read16, m_bus.read32,
            m_bus.write8, m_bus.write16, m_bus.write32
        );

        if (needs_flush) {
            m_pipeline.flush(m_regs, m_bus.read32, m_bus.read16);
        }
    }
}
```

### Validação recomendada após a correção

Testar manualmente uma instrução `B`/`BL` simples e conferir se o PC calculado bate com o esperado — o pipeline de 3 estágios do ARM espera `regs.get_pc()` valendo "endereço da instrução + 8" no momento da execução (usado em `arm_alu.hpp` e `arm_branch.hpp`), e esse caminho nunca foi exercitado de fato até agora.

---

## 2. 🔴 CRÍTICO — Loop de frame duplicado e desalinhado

**Arquivo:** `src/platform/emulador.cpp`
**Funções:** `Emulador::run()` e `Emulador::step()`

### O que está errado

`Emulador::step()` já executa um frame **inteiro** internamente (roda até acumular `CYCLES_PER_FRAME` ciclos reais vindos de `memory_bus.consume_cycles()`). Porém `Emulador::run()` trata cada chamada a `step()` como se ela avançasse apenas **8 ciclos fictícios** (`frame_cycles += 8`), e só para de chamar `step()` de novo quando esse contador falso chega a `CYCLES_PER_FRAME` (280.896).

Resultado: `step()` é chamado **280896 / 8 = 35.112 vezes** por iteração do laço externo, e cada uma dessas chamadas já executa um frame completo sozinha. Ou seja, a cada "frame" do `run()`, o emulador processa ~35 mil frames de GBA (quase 10 minutos de tempo emulado) antes de:
- chamar `renderer.renderFrame()` uma única vez, e
- chamar `input.pollEvents()` de novo.

### Por que isso causa tela preta

A janela SDL abre com o framebuffer inicial (`0xFF000000`, preto opaco) e o programa fica preso processando dezenas de milhares de frames antes de desenhar o segundo frame na tela ou reagir a eventos do SO. Na prática, parece congelado/travado.

### Como resolver

Unificar a responsabilidade: `step()` deve executar **um único ciclo/instrução**, e `run()` deve ser o único lugar que acumula ciclos até fechar um frame.

```cpp
// emulador.hpp
void step(); // executa 1 instrução + avança a PPU proporcionalmente

// emulador.cpp
void Emulador::step() {
    debugger.update();
    cpu.step();
    uint64_t cycles_spent = memory_bus.consume_cycles();
    if (cycles_spent == 0) cycles_spent = 1;
    ppu.step(static_cast<uint32_t>(cycles_spent), memory_bus);
}

void Emulador::run() {
    is_running = true;
    const auto frame_duration = std::chrono::nanoseconds(16742706);

    while (is_running) {
        auto frame_start = std::chrono::high_resolution_clock::now();
        input.pollEvents(is_running);

        uint32_t frame_cycles = 0;
        while (frame_cycles < CYCLES_PER_FRAME && is_running) {
            debugger.update();
            cpu.step();
            uint64_t cycles_spent = memory_bus.consume_cycles();
            if (cycles_spent == 0) cycles_spent = 1;
            frame_cycles += static_cast<uint32_t>(cycles_spent);
            ppu.step(static_cast<uint32_t>(cycles_spent), memory_bus);
        }

        const auto& ppu_fb = ppu.get_framebuffer();
        std::copy(ppu_fb.begin(), ppu_fb.end(), std::begin(framebuffer));

        renderer.renderFrame(framebuffer);
        // ... resto do controle de timing (inalterado)
    }
}
```

Isso garante 1 iteração do laço externo = exatamente 1 frame de GBA, com `renderFrame()` e `pollEvents()` no ritmo correto (~60 Hz).

---

## 3. 🟠 ALTO — Registrador IF sem semântica write-1-to-clear

**Arquivo:** `src/core/memory/bus.cpp`
**Função:** `Bus::write16_internal()` (região `0x04`, I/O Registers)

### O que está errado

No hardware real, escrever `1` num bit de `REG_IF` (0x04000202) **limpa** aquele bit (reconhece/"ack" a interrupção); não é uma escrita direta comum. A implementação atual trata todo o espaço de I/O, incluindo o `IF`, como escrita simples:

```cpp
case 0x04:
    if (offset < IO_SIZE) {
        *reinterpret_cast<uint16_t*>(&mem.io_regs[offset & (IO_SIZE - 1)]) = val;
    }
    break;
```

### Por que isso importa

O idiom padrão usado por praticamente todo jogo comercial na rotina de interrupção é `REG_IF = REG_IF;` (ack de tudo que está pendente). Com a implementação atual, isso não limpa nada — o bit continua setado, e jogos que dependem de `VBlankIntrWait`/IRQ (a maioria) ficam presos aguardando uma interrupção que nunca é reconhecida como tratada.

### Como resolver

```cpp
case 0x04:
    if (offset == 0x0202) { // REG_IF: write-1-to-clear
        uint16_t current = *reinterpret_cast<uint16_t*>(&mem.io_regs[offset]);
        *reinterpret_cast<uint16_t*>(&mem.io_regs[offset]) = current & ~val;
    } else if (offset < IO_SIZE) {
        *reinterpret_cast<uint16_t*>(&mem.io_regs[offset & (IO_SIZE - 1)]) = val;
    }
    break;
```

Replicar a mesma lógica em `write8`/`write32` (ambos passam por `write16_internal`, mas escritas de 32 bits cobrem `IE`+`IF` juntos — vale um caso especial equivalente).

---

## 4. 🟡 MÉDIO — `request_interrupt` polui a contagem de ciclos

**Arquivo:** `src/core/memory/bus.cpp`
**Função:** `Bus::request_interrupt()`

### O que está errado

```cpp
void Bus::request_interrupt(uint16_t interrupt_mask) {
    uint16_t current_if = read16(0x04000202);
    write16(0x04000202, current_if | interrupt_mask);
}
```

`read16`/`write16` chamam `calculate_access_cycles()` internamente. Toda vez que o PPU sinaliza VBlank/HBlank via `request_interrupt`, isso soma ciclos de wait-state ao `pending_cycles` como se fosse um acesso normal da CPU à memória — desalinhando a contagem de ciclos por frame usada em `Emulador::step()`.

### Como resolver

Usar um caminho interno que escreve o registrador sem contar ciclos:

```cpp
void Bus::request_interrupt(uint16_t interrupt_mask) {
    uint16_t current_if = *reinterpret_cast<uint16_t*>(&mem.io_regs[0x0202]);
    write16_internal(0x04000202, current_if | interrupt_mask);
}
```

---

## 5. 🟡 MÉDIO — `KEYINPUT` nunca sincronizado com a memória mapeada

**Arquivos:** `src/platform/input.cpp`, `src/platform/emulador.cpp`

### O que está errado

`Input::getKeyInputRegister()` retorna corretamente `0x03FF` (todos os botões soltos, lógica invertida do GBA), mas nada em `emulador.cpp` ou `bus.cpp` escreve esse valor no endereço mapeado `0x04000130`. Como `io_regs` começa zerado, o jogo lê `KEYINPUT = 0x0000`, que na lógica invertida significa **todos os botões pressionados ao mesmo tempo**.

### Por que isso importa

Não impede a tela de acender, mas vai causar bugs assim que o vídeo funcionar (menus pulando sozinhos, jogo reagindo a "botões fantasma").

### Como resolver

Sincronizar a cada frame, por exemplo em `Emulador::step()`/`run()`:

```cpp
*reinterpret_cast<uint16_t*>(&memory_map.io_regs[0x130]) = input.getKeyInputRegister();
```

---

## 6. 🟢 BAIXO — Shift incompleto em offset de registrador (LDR/STR)

**Arquivo:** `src/core/cpu/arm/arm_load_store.hpp`
**Função:** `ARMLoadStore::execute_single()`

### O que está errado

```cpp
uint8_t shift_type = (opcode >> 5) & 0x03;
uint8_t shift_imm = (opcode >> 7) & 0x1F;
if (shift_imm > 0) {
    if (shift_type == 0) offset <<= shift_imm;      // LSL — ok
    else if (shift_type == 1) offset >>= shift_imm; // LSR — ok
    // ASR (shift_type == 2) e ROR (shift_type == 3): não tratados
}
```

Também não trata o caso `shift_imm == 0` com LSR/ASR, que no ARM significa shift de 32 bits (não "sem shift").

### Por que isso importa

Não é a causa da tela preta, mas vai gerar endereços de memória incorretos em instruções `LDR`/`STR` com offset de registrador deslocado — padrão comum em acesso a arrays gerado por compiladores. Vale corrigir depois que a execução básica estiver funcionando.

### Como resolver

Espelhar a lógica completa de `evaluate_shift()` (já implementada corretamente em `arm_alu.hpp`) também aqui, incluindo os 4 tipos de shift e os casos de shift-by-zero equivalente a 32.

---

## 7. 🟢 BAIXO — APU mixa áudio mesmo com master enable desligado

**Arquivo:** `src/core/audio/apu.cpp`
**Função:** `APU::get_audio_samples()`

### O que está errado

A função mixa e envia amostras independentemente do bit 7 (`Master Sound Enable`) de `soundcnt_x`. Não trava nada, mas gera ruído indevido se o jogo desligar o áudio deliberadamente (comum em telas de pausa/menus).

### Como resolver

```cpp
void APU::get_audio_samples(std::vector<int16_t>& output_buffer) {
    if (!(soundcnt_x & 0x80)) {
        output_buffer.push_back(0);
        output_buffer.push_back(0);
        return;
    }
    // ... mixagem normal
}
```

---

## Pendências — Ainda não analisado

- **`src/core/video/ppu.cpp`**: não foi enviado até o momento (só `ppu.hpp` e `video_types.hpp`). É onde ficam `render_scanline`, `render_mode0/3/4/5`, `step()` do PPU e `read_register`/`write_register`. Mesmo após corrigir os 2 bugs críticos deste relatório, se a tela continuar preta, o próximo lugar a investigar é esse arquivo — em especial:
  - Se `forced_blank` (`DISPCNT` bit 7) está sendo respeitado corretamente.
  - Se `step()` de fato varre as 228 scanlines e dispara `request_interrupt` no VBlank/HBlank.
  - Se `render_scanline()` está escrevendo no `framebuffer` interno nos índices corretos.

---

## Ordem de Prioridade Recomendada

1. **Corrigir `step_arm()`** (item 1) — sem isso, nada mais importa.
2. **Corrigir o loop de frame** (item 2) — sem isso, o programa parece travado mesmo com a CPU funcionando.
3. Recompilar e testar com uma ROM.
4. Se a tela acender mas o jogo travar/bugar: aplicar itens 3, 4 e 5 (IF, ciclos de interrupção, input).
5. Se a tela continuar preta: revisar `ppu.cpp` (pendente).
6. Itens 6 e 7 podem esperar — são refinamentos de precisão, não bloqueiam a renderização básica.
