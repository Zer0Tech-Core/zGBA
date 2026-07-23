#pragma once

#include <cstdint>
#include "../arm7tdmi/registers.hpp"

namespace zGBA::CPU::ARM7TDMI::Thumb {

class ThumbBranch {
public:
    static bool eval_condition(const Registers& regs, uint8_t cond) {
        bool n = regs.is_flag_set(Registers::Flag::N);
        bool z = regs.is_flag_set(Registers::Flag::Z);
        bool c = regs.is_flag_set(Registers::Flag::C);
        bool v = regs.is_flag_set(Registers::Flag::V);

        switch (cond) {
            case 0x0: return z;                   // EQ
            case 0x1: return !z;                  // NE
            case 0x2: return c;                   // CS / HS
            case 0x3: return !c;                  // CC / LO
            case 0x4: return n;                   // MI
            case 0x5: return !n;                  // PL
            case 0x6: return v;                   // VS
            case 0x7: return !v;                  // VC
            case 0x8: return c && !z;             // HI
            case 0x9: return !c || z;             // LS
            case 0xA: return n == v;              // GE
            case 0xB: return n != v;              // LT
            case 0xC: return !z && (n == v);      // GT
            case 0xD: return z || (n != v);       // LE
            default: return true;
        }
    }

    // Format 16: Conditional Branch
    static bool execute_cond_branch(Registers& regs, uint16_t opcode) {
        uint8_t cond = (opcode >> 8) & 0x0F;
        int8_t offset = static_cast<int8_t>(opcode & 0xFF);

        if (eval_condition(regs, cond)) {
            uint32_t target = regs.get_pc() + (offset << 1);
            regs.set_pc(target);
            return true; // Pipeline precisa ser limpo (flush)
        }
        return false;
    }

    // Format 18: Unconditional Branch
    static void execute_uncond_branch(Registers& regs, uint16_t opcode) {
        int16_t offset = (opcode & 0x07FF);
        if (offset & 0x0400) offset |= 0xF800; // Sign extend (11-bit)

        uint32_t target = regs.get_pc() + (offset << 1);
        regs.set_pc(target);
    }

    // Format 5: Branch Exchange (BX)
    static bool execute_bx(Registers& regs, uint16_t opcode) {
        bool h2 = (opcode >> 6) & 1;
        uint8_t rs = ((opcode >> 3) & 0x07) | (h2 ? 8 : 0);

        uint32_t target = regs.read(rs);
        bool thumb_mode = target & 1;

        // Ajuste aqui: define a flag T do CPSR
        regs.set_flag(Registers::Flag::T, thumb_mode);
        regs.set_pc(target & ~1U);
        return true;
    }

    // Format 19: Long Branch with Link (BL)
    static void execute_bl_prefix(Registers& regs, uint16_t opcode) {
        int32_t offset = opcode & 0x07FF;
        if (offset & 0x0400) offset |= 0xFFFFF800; // Sign extend (11-bit)

        uint32_t target = regs.get_pc() + (offset << 12);
        regs.write(14, target); // LR = PC + (Offset High << 12)
    }

    static void execute_bl_suffix(Registers& regs, uint16_t opcode) {
        uint32_t offset = opcode & 0x07FF;
        uint32_t next_instr = (regs.get_pc() - 2) | 1; // PC atual do pipeline - 2 com bit Thumb

        uint32_t target = regs.read(14) + (offset << 1);
        regs.write(14, next_instr); // LR guarda o retorno
        regs.set_pc(target);
    }
};

} // namespace zGBA::CPU::ARM7TDMI::Thumb