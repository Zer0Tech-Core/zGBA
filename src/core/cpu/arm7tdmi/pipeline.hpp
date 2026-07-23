#pragma once

#include <cstdint>
#include <functional>
#include "registers.hpp"

namespace zGBA::CPU::ARM7TDMI {

// Estrutura para armazenar o estado das instruções dentro do pipeline de 3 estágios[cite: 2]
struct PipelineStage {
    uint32_t instruction{0};
    uint32_t address{0};
    bool valid{false};
};

class Pipeline {
public:
    Pipeline() = default;

    // Esvazia e recarrega o pipeline (usado após Branches, Exceções e alterações no PC)
    using MemoryRead32 = std::function<uint32_t(uint32_t)>;
    using MemoryRead16 = std::function<uint16_t(uint32_t)>;

    void flush(Registers& regs, const MemoryRead32& read32, const MemoryRead16& read16);

    // Avança 1 ciclo do pipeline: Fetch -> Decode -> Execute[cite: 2]
    // Retorna o estágio pronto para ser executado no ciclo atual
    PipelineStage step(Registers& regs, const MemoryRead32& read32, const MemoryRead16& read16);

    [[nodiscard]] const PipelineStage& get_execute_stage() const { return execute_stage; }
    [[nodiscard]] const PipelineStage& get_decode_stage() const { return decode_stage; }

private:
    PipelineStage fetch_stage{};
    PipelineStage decode_stage{};
    PipelineStage execute_stage{};
};

} // namespace zGBA::CPU::ARM7TDMI