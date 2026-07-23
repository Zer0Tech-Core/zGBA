#pragma once

#include <functional>
#include "src/core/cpu/arm7tdmi/registers.hpp"
#include "src/core/cpu/arm7tdmi/exceptions.hpp"
#include "src/core/cpu/arm7tdmi/pipeline.hpp"
#include "src/core/cpu/thumb/thumb_decoder.hpp"

namespace zGBA::CPU::ARM7TDMI {

/**
 * @brief Classe principal que orquestra o ciclo de vida e execução do núcleo ARM7TDMI.
 */
class CPU {
public:
    // Definição das funções de acesso à memória via Barramento
    using Read8Func   = std::function<uint8_t(uint32_t)>;
    using Read16Func  = std::function<uint16_t(uint32_t)>;
    using Read32Func  = std::function<uint32_t(uint32_t)>;
    using Write8Func  = std::function<void(uint32_t, uint8_t)>;
    using Write16Func = std::function<void(uint32_t, uint16_t)>;
    using Write32Func = std::function<void(uint32_t, uint32_t)>;

    struct MemoryBus {
        Read8Func   read8;
        Read16Func  read16;
        Read32Func  read32;
        Write8Func  write8;
        Write16Func write16;
        Write32Func write32;
    };

    CPU() = default;
    explicit CPU(MemoryBus bus);

    /**
     * @brief Conecta a CPU ao Barramento de Memória.
     */
    void set_bus(MemoryBus bus);

    /**
     * @brief Reseta o estado da CPU para as condições iniciais de hardware (Boot/Reset Vector).
     */
    void reset();

    /**
     * @brief Executa um ciclo/instrução do pipeline (ARM ou Thumb).
     */
    void step();

    /**
     * @brief Dispara uma exceção de hardware/software na CPU.
     */
    void raise_exception(ExceptionType type);

    // Acesso ao estado interno (para depuração, testes e PPU/DMA)
    [[nodiscard]] Registers& get_registers() { return m_regs; }
    [[nodiscard]] const Registers& get_registers() const { return m_regs; }

    [[nodiscard]] Pipeline& get_pipeline() { return m_pipeline; }
    [[nodiscard]] const Pipeline& get_pipeline() const { return m_pipeline; }

private:
    Registers m_regs;
    Pipeline  m_pipeline;
    MemoryBus m_bus;

    void step_arm();
    void step_thumb();
};

} // namespace zGBA::CPU::ARM7TDMI