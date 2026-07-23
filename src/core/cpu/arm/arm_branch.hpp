// src/cpu/arm/arm_branch.hpp
#pragma once

#include <cstdint>
#include "../arm7tdmi/registers.hpp"

namespace zGBA::CPU::ARM7TDMI::ARM {

class ARMBranch {
public:
    static bool execute_branch(uint32_t opcode, Registers& regs) {
        bool link = (opcode >> 24) & 1;
        
        // Extrai o offset de 24 bits com sinal (sign extension)
        int32_t offset = opcode & 0x00FFFFFF;
        if (offset & 0x00800000) {
            offset |= 0xFF000000; // Extensão de sinal para 32 bits
        }
        
        // Desloca o offset em 2 bits à esquerda (palavras de 32 bits)
        offset <<= 2;

        // Se for BL, salva o endereço de retorno (PC atual - 4, considerando o pipeline de +8)
        if (link) {
            uint32_t return_address = regs.get_pc() - 4;
            regs.write(14, return_address); // LR (R14)
        }

        // Atualiza o PC: PC atual (que é instruction_address + 8) + offset do branch
        uint32_t current_pc = regs.get_pc();
        uint32_t target_address = current_pc + offset;
        regs.write(15, target_address);

        return true; // Retorna true para indicar que o PC mudou (exige flush do pipeline)
    }

    // Branch and Exchange (BX Rm)
    static bool execute_bx(uint32_t opcode, Registers& regs) {
        uint8_t rm_idx = opcode & 0x0F;
        uint32_t target = regs.read(rm_idx);

        // O bit 0 determina o estado: 1 = Thumb, 0 = ARM
        bool thumb_mode = target & 1;
        regs.set_thumb(thumb_mode);

        // O endereço alvo limpa o bit 0
        regs.write(15, target & ~1);

        return true; // Flush do pipeline
    }
};

} // namespace zGBA::CPU::ARM7TDMI::ARM