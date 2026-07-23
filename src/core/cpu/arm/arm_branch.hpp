// src/cpu/arm/arm_branch.hpp
#pragma once

#include <cstdint>
#include "../arm7tdmi/registers.hpp"

namespace zGBA::CPU::ARM7TDMI::ARM {

class ARMBranch {
public:
    // B e BL (Branch e Branch with Link)
    static bool execute_branch(uint32_t opcode, Registers& regs) {
        bool link = (opcode >> 24) & 1;
        int32_t offset = opcode & 0x00FFFFFF;

        // Sign extend de 24 para 32 bits
        if (offset & 0x00800000) {
            offset |= 0xFF000000;
        }

        // Offset é rotacionado 2 bits para a esquerda (alinhamento de word)
        offset <<= 2;

        uint32_t current_pc = regs.get_pc();
        if (link) {
            // Salva o PC da instrução seguinte no LR
            regs.write(14, current_pc - 4);
        }

        // O PC armazenado já está +8 no pipeline
        regs.set_pc(current_pc + offset);
        return true; // Exige flush do pipeline
    }

    // BX (Branch Exchange)
    static bool execute_bx(uint32_t opcode, Registers& regs) {
        uint8_t rm_idx = opcode & 0x0F;
        uint32_t target = regs.read(rm_idx);

        // Bit 0 determina o novo estado (0: ARM, 1: Thumb)
        bool thumb_mode = target & 1;
        regs.set_flag(Registers::Flag::T, thumb_mode);
        regs.set_pc(target & ~1U); // Alinha o endereço

        return true; // Exige flush do pipeline
    }
};

} // namespace zGBA::CPU::ARM7TDMI::ARM