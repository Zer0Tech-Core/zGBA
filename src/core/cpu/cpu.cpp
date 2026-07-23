#include "src/core/cpu/cpu.hpp"
#include "src/core/cpu/arm/arm_decoder.hpp"

namespace zGBA::CPU::ARM7TDMI {

CPU::CPU(MemoryBus bus) : m_bus(std::move(bus)) {}

void CPU::set_bus(MemoryBus bus) {
    m_bus = std::move(bus);
}

void CPU::reset() {
    // 1. Configura registradores no estado pós-reset (Modo Supervisor, ARM, IRQ/FIQ desabilitados)
    m_regs.reset();

    // 2. Preenche o pipeline a partir do vetor de Reset (0x00000000)
    if (m_bus.read32 && m_bus.read16) {
        m_pipeline.flush(m_regs, m_bus.read32, m_bus.read16);
    }
}

void CPU::raise_exception(ExceptionType type) {
    Exceptions::raise(m_regs, type);
    
    // Recarrega o pipeline no novo endereço do vetor de exceção
    if (m_bus.read32 && m_bus.read16) {
        m_pipeline.flush(m_regs, m_bus.read32, m_bus.read16);
    }
}

void CPU::step() {
    // Alterna a execução de acordo com o flag Thumb do CPSR
    if (m_regs.is_thumb()) {
        step_thumb();
    } else {
        step_arm();
    }
}

void CPU::step_thumb() {
    // Em modo Thumb, o PC no estágio de busca/execução considera o deslocamento de +4 (2 instruções de 16 bits)
    uint32_t current_pc = m_regs.get_pc();
    uint32_t execute_pc = current_pc - 4;
    
    uint16_t instruction = m_bus.read16(execute_pc);

    // Decodifica e executa a instrução Thumb
    bool needs_flush = Thumb::ThumbDecoder::decode_and_execute(
        instruction,
        m_regs,
        m_bus.read8,
        m_bus.read16,
        m_bus.read32,
        m_bus.write8,
        m_bus.write16,
        m_bus.write32
    );

    if (needs_flush) {
        m_pipeline.flush(m_regs, m_bus.read32, m_bus.read16);
    } else {
        // Avança PC em +2 bytes (1 instrução Thumb)
        m_regs.set_pc(m_regs.get_pc() + 2);
    }
}

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

} // namespace zGBA::CPU::ARM7TDMI