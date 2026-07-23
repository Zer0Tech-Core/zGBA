#include "arm_decoder.hpp"
#include "arm_alu.hpp"
#include "arm_branch.hpp"
#include "arm_load_store.hpp"
#include "arm_coprocessor.hpp"

namespace zGBA::CPU::ARM7TDMI::ARM {

template<typename R8, typename R16, typename R32, typename W8, typename W16, typename W32>
bool ARMDecoder::decode_and_execute(uint32_t opcode, Registers& regs, 
                                    R8 r8, R16 r16, R32 r32, 
                                    W8 w8, W16 w16, W32 w32) {
    uint8_t cond = (opcode >> 28) & 0x0F;

    // Se a condição falhar, a instrução age como NOP de 1 ciclo
    if (!check_condition(cond, regs)) {
        return false;
    }

    // Branch Exchange (BX)
    if ((opcode & 0x0FFFFFF0) == 0x012FFF10) {
        return ARMBranch::execute_bx(opcode, regs);
    }

    // Multiplicações (MUL / MLA)
    if ((opcode & 0x0FC000F0) == 0x00000090) {
        return ARMALU::execute_multiply(opcode, regs);
    }

    // Halfword & Signed Data Transfer
    if ((opcode & 0x0E000090) == 0x00000090) {
        return ARMLoadStore::execute_extra(opcode, regs, r8, r16, r32, w16, w32);
    }

    // Single Data Transfer (LDR, STR)
    if ((opcode & 0x0C000000) == 0x04000000) {
        return ARMLoadStore::execute_single(opcode, regs, r8, r32, w8, w32);
    }

    // Block Data Transfer (LDM, STM)
    if ((opcode & 0x0E000000) == 0x08000000) {
        return ARMLoadStore::execute_block(opcode, regs, r32, w32);
    }

    // Branch / Branch with Link (B, BL)
    if ((opcode & 0x0E000000) == 0x0A000000) {
        return ARMBranch::execute_branch(opcode, regs);
    }

    // Coprocessor instructions (MCR, MRC, CDP)
    if ((opcode & 0x0C000000) == 0x0C000000) {
        return ARMCoprocessor::execute(opcode, regs);
    }

    // Data Processing (ALU) - Fallback para instruções ALU genéricas
    if ((opcode & 0x0C000000) == 0x00000000) {
        return ARMALU::execute_data_processing(opcode, regs);
    }

    return false;
}

// Instanciação explícita do template para uso genérico
template bool ARMDecoder::decode_and_execute(
    uint32_t, Registers&, 
    std::function<uint8_t(uint32_t)>, std::function<uint16_t(uint32_t)>, std::function<uint32_t(uint32_t)>,
    std::function<void(uint32_t, uint8_t)>, std::function<void(uint32_t, uint16_t)>, std::function<void(uint32_t, uint32_t)>
);

} // namespace zGBA::CPU::ARM7TDMI::ARM