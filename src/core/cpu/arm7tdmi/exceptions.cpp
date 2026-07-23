#include "exceptions.hpp"

namespace zGBA::CPU::ARM7TDMI {

void Exceptions::raise(Registers& regs, ExceptionType type) {
    const uint32_t old_cpsr = regs.cpsr;
    const bool was_thumb = regs.is_thumb();
    
    // O PC no pipeline de 3 estágios está 2 instruções à frente (Execute_Addr + 8 no ARM, Execute_Addr + 4 no Thumb)[cite: 2]
    const uint32_t current_pc = regs.get_pc();

    Mode target_mode = Mode::Supervisor;
    uint32_t return_address = current_pc;

    // Cálculo do Link Register (LR) e definição do modo alvo de acordo com o vetor[cite: 2]
    switch (type) {
        case ExceptionType::Reset:
            target_mode = Mode::Supervisor; //[cite: 2]
            return_address = 0;
            break;

        case ExceptionType::UndefinedInstruction:
            target_mode = Mode::Undefined; //[cite: 2]
            return_address = was_thumb ? (current_pc - 2) : (current_pc - 4); // LR = Próxima instrução[cite: 2]
            break;

        case ExceptionType::SoftwareInterrupt:
            target_mode = Mode::Supervisor; //[cite: 2]
            return_address = was_thumb ? (current_pc - 2) : (current_pc - 4); // LR = Próxima instrução[cite: 2]
            break;

        case ExceptionType::PrefetchAbort:
            target_mode = Mode::Abort; //[cite: 2]
            return_address = current_pc - 4; // LR = Instrução que falhou + 4 (SUBS PC, LR, #4)[cite: 2]
            break;

        case ExceptionType::DataAbort:
            target_mode = Mode::Abort; //[cite: 2]
            return_address = current_pc;     // LR = Instrução que falhou + 8 (SUBS PC, LR, #8)[cite: 2]
            break;

        case ExceptionType::IRQ:
            target_mode = Mode::IRQ; //[cite: 2]
            return_address = current_pc - 4; // LR = Próxima instrução + 4 (SUBS PC, LR, #4)[cite: 2]
            break;

        case ExceptionType::FIQ:
            target_mode = Mode::FIQ; //[cite: 2]
            return_address = current_pc - 4; // LR = Próxima instrução + 4 (SUBS PC, LR, #4)[cite: 2]
            break;
    }

    // 1. Troca para o modo da exceção[cite: 2]
    regs.set_mode(target_mode);

    // 2. Salva o CPSR anterior no SPSR do novo modo[cite: 2]
    regs.write_spsr(old_cpsr);

    // 3. Salva o endereço de retorno no LR do novo modo[cite: 2]
    regs.write(14, return_address);

    // 4. Desabilita IRQ (e FIQ se for Reset ou FIQ)[cite: 2]
    regs.set_flag(Registers::Flag::I, true); //[cite: 2]
    if (type == ExceptionType::Reset || type == ExceptionType::FIQ) {
        regs.set_flag(Registers::Flag::F, true); //[cite: 2]
    }

    // 5. Força o estado para ARM Mode (T = 0)[cite: 2]
    regs.set_flag(Registers::Flag::T, false); //[cite: 2]

    // 6. Altera o PC para o endereço do Vetor de Exceção[cite: 2]
    regs.set_pc(static_cast<uint32_t>(type)); //[cite: 2]
}

void Exceptions::restore_from_spsr(Registers& regs) {
    uint32_t spsr_val = regs.read_spsr();
    
    // Restaura CPSR a partir do SPSR (restaura modo anterior, flags e estado Thumb/ARM)[cite: 2]
    regs.cpsr = spsr_val;
    
    // Sincroniza o banco de registradores ativo com o novo modo restaurado
    regs.set_mode(regs.get_mode());
}

} // namespace zGBA::CPU::ARM7TDMI