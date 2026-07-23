#pragma once

#include <cstdint>
#include "../arm7tdmi/registers.hpp"

namespace zGBA::CPU::ARM7TDMI::Thumb {

class ThumbALU {
public:
    // Format 1: Move Shifted Register (LSL, LSR, ASR)
    static void execute_shift_imm(Registers& regs, uint16_t opcode) {
        uint8_t op = (opcode >> 11) & 0x03;
        uint8_t offset = (opcode >> 6) & 0x1F;
        uint8_t rs = (opcode >> 3) & 0x07;
        uint8_t rd = opcode & 0x07;

        uint32_t val = regs.read(rs);
        uint32_t result = val;
        bool carry = regs.is_flag_set(Registers::Flag::C);

        switch (op) {
            case 0: // LSL
                if (offset == 0) {
                    result = val;
                } else {
                    carry = (val >> (32 - offset)) & 1;
                    result = val << offset;
                }
                break;
            case 1: // LSR
                if (offset == 0) { // LSR #32
                    carry = (val >> 31) & 1;
                    result = 0;
                } else {
                    carry = (val >> (offset - 1)) & 1;
                    result = val >> offset;
                }
                break;
            case 2: // ASR
                if (offset == 0) { // ASR #32
                    carry = (val >> 31) & 1;
                    result = static_cast<int32_t>(val) >> 31;
                } else {
                    carry = (val >> (offset - 1)) & 1;
                    result = static_cast<int32_t>(val) >> offset;
                }
                break;
        }

        regs.write(rd, result);
        regs.set_flag(Registers::Flag::N, result & 0x80000000);
        regs.set_flag(Registers::Flag::Z, result == 0);
        regs.set_flag(Registers::Flag::C, carry);
    }

    // Format 2: Add/Subtract Register or Immediate
    static void execute_add_sub(Registers& regs, uint16_t opcode) {
        bool is_imm = (opcode >> 10) & 1;
        bool is_sub = (opcode >> 9) & 1;
        uint8_t rn_imm = (opcode >> 6) & 0x07;
        uint8_t rs = (opcode >> 3) & 0x07;
        uint8_t rd = opcode & 0x07;

        uint32_t op1 = regs.read(rs);
        uint32_t op2 = is_imm ? rn_imm : regs.read(rn_imm);
        uint32_t result;

        if (is_sub) {
            result = op1 - op2;
            regs.set_flag(Registers::Flag::C, op1 >= op2);
            regs.set_flag(Registers::Flag::V, ((op1 ^ op2) & (op1 ^ result)) >> 31);
        } else {
            result = op1 + op2;
            regs.set_flag(Registers::Flag::C, result < op1);
            regs.set_flag(Registers::Flag::V, (~(op1 ^ op2) & (op1 ^ result)) >> 31);
        }

        regs.write(rd, result);
        regs.set_flag(Registers::Flag::N, result & 0x80000000);
        regs.set_flag(Registers::Flag::Z, result == 0);
    }

    // Format 3: Move/Compare/Add/Subtract Immediate
    static void execute_imm_op(Registers& regs, uint16_t opcode) {
        uint8_t op = (opcode >> 11) & 0x03;
        uint8_t rd = (opcode >> 8) & 0x07;
        uint32_t imm = opcode & 0xFF;

        uint32_t val = regs.read(rd);
        uint32_t result = 0;

        switch (op) {
            case 0: // MOV
                result = imm;
                regs.write(rd, result);
                break;
            case 1: // CMP
                result = val - imm;
                regs.set_flag(Registers::Flag::C, val >= imm);
                regs.set_flag(Registers::Flag::V, ((val ^ imm) & (val ^ result)) >> 31);
                break;
            case 2: // ADD
                result = val + imm;
                regs.set_flag(Registers::Flag::C, result < val);
                regs.set_flag(Registers::Flag::V, (~(val ^ imm) & (val ^ result)) >> 31);
                regs.write(rd, result);
                break;
            case 3: // SUB
                result = val - imm;
                regs.set_flag(Registers::Flag::C, val >= imm);
                regs.set_flag(Registers::Flag::V, ((val ^ imm) & (val ^ result)) >> 31);
                regs.write(rd, result);
                break;
        }

        regs.set_flag(Registers::Flag::N, result & 0x80000000);
        regs.set_flag(Registers::Flag::Z, result == 0);
    }

    // Format 4: ALU Operations (AND, EOR, LSL, LSR, ASR, ADC, SBC, ROR, TST, NEG, CMP, CMN, ORR, MUL, BIC, MVN)
    static void execute_alu_ops(Registers& regs, uint16_t opcode) {
        uint8_t op = (opcode >> 6) & 0x0F;
        uint8_t rs = (opcode >> 3) & 0x07;
        uint8_t rd = opcode & 0x07;

        uint32_t val_d = regs.read(rd);
        uint32_t val_s = regs.read(rs);
        uint32_t result = 0;

        switch (op) {
            case 0x0: result = val_d & val_s; regs.write(rd, result); break; // AND
            case 0x1: result = val_d ^ val_s; regs.write(rd, result); break; // EOR
            case 0x5: { // ADC
                uint64_t c_in = regs.is_flag_set(Registers::Flag::C) ? 1 : 0;
                uint64_t res64 = static_cast<uint64_t>(val_d) + val_s + c_in;
                result = static_cast<uint32_t>(res64);
                regs.set_flag(Registers::Flag::C, res64 > 0xFFFFFFFF);
                regs.set_flag(Registers::Flag::V, (~(val_d ^ val_s) & (val_d ^ result)) >> 31);
                regs.write(rd, result);
                break;
            }
            case 0x6: { // SBC
                uint32_t c_in = regs.is_flag_set(Registers::Flag::C) ? 1 : 0;
                uint64_t res64 = static_cast<uint64_t>(val_d) - val_s - (1 - c_in);
                result = static_cast<uint32_t>(res64);
                regs.set_flag(Registers::Flag::C, val_d >= (val_s + (1 - c_in)));
                regs.set_flag(Registers::Flag::V, ((val_d ^ val_s) & (val_d ^ result)) >> 31);
                regs.write(rd, result);
                break;
            }
            case 0x8: result = val_d & val_s; break; // TST (não escreve Rd)
            case 0x9: // NEG
                result = 0 - val_s;
                regs.set_flag(Registers::Flag::C, 0 >= val_s);
                regs.set_flag(Registers::Flag::V, (val_s & result) >> 31);
                regs.write(rd, result);
                break;
            case 0xA: // CMP
                result = val_d - val_s;
                regs.set_flag(Registers::Flag::C, val_d >= val_s);
                regs.set_flag(Registers::Flag::V, ((val_d ^ val_s) & (val_d ^ result)) >> 31);
                break;
            case 0xB: // CMN
                result = val_d + val_s;
                regs.set_flag(Registers::Flag::C, result < val_d);
                regs.set_flag(Registers::Flag::V, (~(val_d ^ val_s) & (val_d ^ result)) >> 31);
                break;
            case 0xC: result = val_d | val_s; regs.write(rd, result); break; // ORR
            case 0xD: result = val_d * val_s; regs.write(rd, result); break; // MUL
            case 0xE: result = val_d & ~val_s; regs.write(rd, result); break; // BIC
            case 0xF: result = ~val_s; regs.write(rd, result); break; // MVN
        }

        regs.set_flag(Registers::Flag::N, result & 0x80000000);
        regs.set_flag(Registers::Flag::Z, result == 0);
    }

    // Format 5: Hi Register Operations (ADD, CMP, MOV)
    static void execute_hi_reg_op(Registers& regs, uint16_t opcode) {
        uint8_t op = (opcode >> 8) & 0x03;
        bool h1 = (opcode >> 7) & 1;
        bool h2 = (opcode >> 6) & 1;
        uint8_t rs = ((opcode >> 3) & 0x07) | (h2 ? 8 : 0);
        uint8_t rd = (opcode & 0x07) | (h1 ? 8 : 0);

        uint32_t val_d = regs.read(rd);
        uint32_t val_s = regs.read(rs);

        switch (op) {
            case 0: // ADD
                regs.write(rd, val_d + val_s);
                break;
            case 1: { // CMP
                uint32_t res = val_d - val_s;
                regs.set_flag(Registers::Flag::N, res & 0x80000000);
                regs.set_flag(Registers::Flag::Z, res == 0);
                regs.set_flag(Registers::Flag::C, val_d >= val_s);
                regs.set_flag(Registers::Flag::V, ((val_d ^ val_s) & (val_d ^ res)) >> 31);
                break;
            }
            case 2: // MOV
                regs.write(rd, val_s);
                break;
        }
    }

    // Format 12 & 13: Add SP/PC Immediate
    static void execute_add_sp_pc(Registers& regs, uint16_t opcode) {
        bool is_sp = (opcode >> 11) & 1;
        uint8_t rd = (opcode >> 8) & 0x07;
        uint32_t imm = (opcode & 0xFF) << 2;

        //uint32_t base = is_sp ? regs.read(13) : (regs.get_pc() & ~2);
        uint32_t base = is_sp ? regs.read(13) : (regs.get_pc() & ~3);
        regs.write(rd, base + imm);
    }

    static void execute_adjust_sp(Registers& regs, uint16_t opcode) {
        bool is_sub = (opcode >> 7) & 1;
        uint32_t imm = (opcode & 0x7F) << 2;
        uint32_t sp = regs.read(13);

        regs.write(13, is_sub ? (sp - imm) : (sp + imm));
    }
};

} // namespace zGBA::CPU::ARM7TDMI::Thumb