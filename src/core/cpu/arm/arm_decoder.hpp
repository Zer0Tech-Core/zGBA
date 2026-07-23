#pragma once

#include <cstdint>
#include "../arm7tdmi/registers.hpp"

namespace zGBA::CPU::ARM7TDMI::ARM {

class ARMDecoder {
public:
    // Verifica a condição do prefixo da instrução ARM (bits 31-28)
    static bool check_condition(uint8_t cond, const Registers& regs) {
        bool n = regs.is_flag_set(Registers::Flag::N);
        bool z = regs.is_flag_set(Registers::Flag::Z);
        bool c = regs.is_flag_set(Registers::Flag::C);
        bool v = regs.is_flag_set(Registers::Flag::V);

        switch (cond) {
            case 0x0: return z;                  // EQ
            case 0x1: return !z;                 // NE
            case 0x2: return c;                  // CS / HS
            case 0x3: return !c;                 // CC / LO
            case 0x4: return n;                  // MI
            case 0x5: return !n;                 // PL
            case 0x6: return v;                  // VS
            case 0x7: return !v;                 // VC
            case 0x8: return c && !z;            // HI
            case 0x9: return !c || z;            // LS
            case 0xA: return n == v;             // GE
            case 0xB: return n != v;             // LT
            case 0xC: return !z && (n == v);     // GT
            case 0xD: return z || (n != v);      // LE
            case 0xE: return true;               // AL (Always)
            case 0xF: return false;              // NV (Never / Reserved)
            default: return false;
        }
    }

    template<typename R8, typename R16, typename R32, typename W8, typename W16, typename W32>
    static bool decode_and_execute(uint32_t opcode, Registers& regs, 
                                   R8 r8, R16 r16, R32 r32, 
                                   W8 w8, W16 w16, W32 w32);
};

} // namespace zGBA::CPU::ARM7TDMI::ARM