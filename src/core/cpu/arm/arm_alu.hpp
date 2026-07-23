// src/cpu/arm/arm_alu.hpp
#pragma once

#include <cstdint>
#include "../arm7tdmi/registers.hpp"

namespace zGBA::CPU::ARM7TDMI::ARM {

class ARMALU {
public:
    // Avalia o Barrel Shifter para o operando 2
    static uint32_t evaluate_shift(uint32_t opcode, Registers& regs, bool& carry_out) {
        bool immediate = (opcode >> 25) & 1;
        carry_out = regs.is_flag_set(Registers::Flag::C);

        if (immediate) {
            uint32_t imm = opcode & 0xFF;
            uint32_t rotate = ((opcode >> 8) & 0x0F) * 2;
            if (rotate == 0) return imm;
            
            uint32_t result = (imm >> rotate) | (imm << (32 - rotate));
            carry_out = (result >> 31) & 1;
            return result;
        }

        uint8_t rm_idx = opcode & 0x0F;
        uint32_t rm = regs.read(rm_idx);
        uint8_t shift_type = (opcode >> 5) & 0x03;
        bool register_shift = (opcode >> 4) & 1;

        uint32_t shift_amount = 0;
        if (register_shift) {
            uint8_t rs_idx = (opcode >> 8) & 0x0F;
            shift_amount = regs.read(rs_idx) & 0xFF;
            if (shift_amount == 0) return rm;
        } else {
            shift_amount = (opcode >> 7) & 0x1F;
        }

        switch (shift_type) {
            case 0: // LSL
                if (shift_amount == 0) return rm;
                if (shift_amount < 32) {
                    carry_out = (rm >> (32 - shift_amount)) & 1;
                    return rm << shift_amount;
                } else if (shift_amount == 32) {
                    carry_out = rm & 1;
                    return 0;
                } else {
                    carry_out = 0;
                    return 0;
                }
            case 1: // LSR
                if (!register_shift && shift_amount == 0) shift_amount = 32;
                if (shift_amount < 32) {
                    carry_out = (rm >> (shift_amount - 1)) & 1;
                    return rm >> shift_amount;
                } else if (shift_amount == 32) {
                    carry_out = (rm >> 31) & 1;
                    return 0;
                } else {
                    carry_out = 0;
                    return 0;
                }
            case 2: // ASR
                if (!register_shift && shift_amount == 0) shift_amount = 32;
                if (shift_amount < 32) {
                    carry_out = (static_cast<int32_t>(rm) >> (shift_amount - 1)) & 1;
                    return static_cast<uint32_t>(static_cast<int32_t>(rm) >> shift_amount);
                } else {
                    carry_out = (rm >> 31) & 1;
                    return (rm & 0x80000000U) ? 0xFFFFFFFFU : 0;
                }
            case 3: // ROR / RRX
                if (!register_shift && shift_amount == 0) { // RRX
                    bool old_c = regs.is_flag_set(Registers::Flag::C);
                    carry_out = rm & 1;
                    return (rm >> 1) | (static_cast<uint32_t>(old_c) << 31);
                }
                shift_amount %= 32;
                if (shift_amount == 0) return rm;
                carry_out = (rm >> (shift_amount - 1)) & 1;
                return (rm >> shift_amount) | (rm << (32 - shift_amount));
        }
        return rm;
    }

    // Processamento de dados ALU (ADD, SUB, MOV, CMP, etc.)
    static bool execute_data_processing(uint32_t opcode, Registers& regs) {
        uint8_t op = (opcode >> 21) & 0x0F;
        bool set_flags = (opcode >> 20) & 1;
        uint8_t rn_idx = (opcode >> 16) & 0x0F;
        uint8_t rd_idx = (opcode >> 12) & 0x0F;

        uint32_t rn = regs.read(rn_idx);
        if (rn_idx == 15) rn += 8; // PC adiantado no pipeline

        bool shifter_carry = false;
        uint32_t op2 = evaluate_shift(opcode, regs, shifter_carry);

        uint32_t res = 0;
        bool carry = regs.is_flag_set(Registers::Flag::C);
        bool writeback = true;

        switch (op) {
            case 0x0: res = rn & op2; break;                         // AND
            case 0x1: res = rn ^ op2; break;                         // EOR
            case 0x2: res = rn - op2; break;                         // SUB
            case 0x3: res = op2 - rn; break;                         // RSB
            case 0x4: res = rn + op2; break;                         // ADD
            case 0x5: res = rn + op2 + carry; break;                 // ADC
            case 0x6: res = rn - op2 - (!carry); break;              // SBC
            case 0x7: res = op2 - rn - (!carry); break;              // RSC
            case 0x8: res = rn & op2; writeback = false; break;      // TST
            case 0x9: res = rn ^ op2; writeback = false; break;      // TEQ
            case 0xA: res = rn - op2; writeback = false; break;      // CMP
            case 0xB: res = rn + op2; writeback = false; break;      // CMN
            case 0xC: res = rn | op2; break;                         // ORR
            case 0xD: res = op2; break;                              // MOV
            case 0xE: res = rn & (~op2); break;                      // BIC
            case 0xF: res = ~op2; break;                             // MVN
        }

        if (writeback) {
            regs.write(rd_idx, res);
        }

        if (set_flags) {
            if (rd_idx == 15 && writeback) {
                // Se Rd é R15 e S=1, restaura o SPSR para o CPSR
                regs.restore_spsr();
            } else {
                regs.set_flag(Registers::Flag::N, (res >> 31) & 1);
                regs.set_flag(Registers::Flag::Z, res == 0);

                // Flags C e V de acordo com a instrução
                if (op == 0x2 || op == 0x3 || op == 0x6 || op == 0x7 || op == 0xA) { // Subtrações
                    uint32_t sub_op1 = (op == 0x3 || op == 0x7) ? op2 : rn;
                    uint32_t sub_op2 = (op == 0x3 || op == 0x7) ? rn : op2;
                    regs.set_flag(Registers::Flag::C, sub_op1 >= sub_op2);
                    bool v = ((sub_op1 ^ sub_op2) & (sub_op1 ^ res)) >> 31;
                    regs.set_flag(Registers::Flag::V, v);
                } else if (op == 0x4 || op == 0x5 || op == 0xB) { // Adições
                    regs.set_flag(Registers::Flag::C, res < rn);
                    bool v = (~(rn ^ op2) & (rn ^ res)) >> 31;
                    regs.set_flag(Registers::Flag::V, v);
                } else { // Operações Lógicas
                    regs.set_flag(Registers::Flag::C, shifter_carry);
                }
            }
        }

        return (rd_idx == 15 && writeback); // Retorna true se alterou PC
    }

    // Multiplicações (MUL, MLA, UMULL, UMLAL, SMULL, SMLAL)
    static bool execute_multiply(uint32_t opcode, Registers& regs) {
        bool accum = (opcode >> 21) & 1;
        bool set_flags = (opcode >> 20) & 1;
        uint8_t rd_idx = (opcode >> 16) & 0x0F;
        uint8_t rn_idx = (opcode >> 12) & 0x0F;
        uint8_t rs_idx = (opcode >> 8) & 0x0F;
        uint8_t rm_idx = opcode & 0x0F;

        uint32_t rm = regs.read(rm_idx);
        uint32_t rs = regs.read(rs_idx);
        uint32_t res = rm * rs;

        if (accum) res += regs.read(rn_idx);

        regs.write(rd_idx, res);

        if (set_flags) {
            regs.set_flag(Registers::Flag::N, (res >> 31) & 1);
            regs.set_flag(Registers::Flag::Z, res == 0);
        }
        return false;
    }
};

} // namespace zGBA::CPU::ARM7TDMI::ARM