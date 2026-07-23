#pragma once

#include <cstdint>
#include "../arm7tdmi/registers.hpp"

namespace zGBA::CPU::ARM7TDMI::ARM {

class ARMLoadStore {
public:
    // Single Data Transfer (LDR, STR, LDRB, STRB)
    template<typename R8, typename R32, typename W8, typename W32>
    static bool execute_single(uint32_t opcode, Registers& regs, R8 r8, R32 r32, W8 w8, W32 w32) {
        bool immediate = !((opcode >> 25) & 1);
        bool pre_index = (opcode >> 24) & 1;
        bool add = (opcode >> 23) & 1;
        bool byte_quant = (opcode >> 22) & 1;
        bool writeback = (opcode >> 21) & 1;
        bool load = (opcode >> 20) & 1;

        uint8_t rn_idx = (opcode >> 16) & 0x0F;
        uint8_t rd_idx = (opcode >> 12) & 0x0F;

        uint32_t offset = 0;
        if (immediate) {
            offset = opcode & 0xFFF;
        } else {
            uint8_t rm_idx = opcode & 0x0F;
            offset = regs.read(rm_idx);
            // Suporte a shift simples no offset
            uint8_t shift_type = (opcode >> 5) & 0x03;
            uint8_t shift_imm = (opcode >> 7) & 0x1F;
            if (shift_imm > 0) {
                if (shift_type == 0) offset <<= shift_imm;
                else if (shift_type == 1) offset >>= shift_imm;
            }
        }

        uint32_t base = regs.read(rn_idx);
        if (rn_idx == 15) base += 8;

        uint32_t addr = pre_index ? (add ? base + offset : base - offset) : base;

        if (load) {
            if (byte_quant) {
                regs.write(rd_idx, r8(addr));
            } else {
                // Alinhamento não alinhado em ARM v4T faz ROR
                uint32_t data = r32(addr & ~3U);
                uint32_t align_shift = (addr & 3) * 8;
                if (align_shift > 0) {
                    data = (data >> align_shift) | (data << (32 - align_shift));
                }
                regs.write(rd_idx, data);
            }
        } else {
            uint32_t val = regs.read(rd_idx);
            if (rd_idx == 15) val += 12;
            if (byte_quant) {
                w8(addr, val & 0xFF);
            } else {
                w32(addr & ~3U, val);
            }
        }

        if (!pre_index) {
            base = add ? base + offset : base - offset;
            regs.write(rn_idx, base);
        } else if (writeback) {
            regs.write(rn_idx, addr);
        }

        return (load && rd_idx == 15);
    }

    // Halfword & Signed Data Transfer (LDRH, STRH, LDRSB, LDRSH)
    template<typename R8, typename R16, typename R32, typename W16, typename W32>
    static bool execute_extra(uint32_t opcode, Registers& regs, R8 r8, R16 r16, R32 r32, W16 w16, W32 w32) {
        bool pre_index = (opcode >> 24) & 1;
        bool add = (opcode >> 23) & 1;
        bool immediate = (opcode >> 22) & 1;
        bool writeback = (opcode >> 21) & 1;
        bool load = (opcode >> 20) & 1;
        uint8_t op_type = (opcode >> 5) & 0x03; // 1: Unsigned H, 2: Signed B, 3: Signed H

        uint8_t rn_idx = (opcode >> 16) & 0x0F;
        uint8_t rd_idx = (opcode >> 12) & 0x0F;

        uint32_t offset = 0;
        if (immediate) {
            offset = ((opcode >> 4) & 0xF0) | (opcode & 0x0F);
        } else {
            uint8_t rm_idx = opcode & 0x0F;
            offset = regs.read(rm_idx);
        }

        uint32_t base = regs.read(rn_idx);
        if (rn_idx == 15) base += 8;

        uint32_t addr = pre_index ? (add ? base + offset : base - offset) : base;

        if (load) {
            uint32_t val = 0;
            if (op_type == 1) { // LDRH
                val = r16(addr & ~1U);
            } else if (op_type == 2) { // LDRSB
                int8_t sbyte = static_cast<int8_t>(r8(addr));
                val = static_cast<uint32_t>(static_cast<int32_t>(sbyte));
            } else if (op_type == 3) { // LDRSH
                int16_t shalf = static_cast<int16_t>(r16(addr & ~1U));
                val = static_cast<uint32_t>(static_cast<int32_t>(shalf));
            }
            regs.write(rd_idx, val);
        } else {
            if (op_type == 1) { // STRH
                w16(addr & ~1U, regs.read(rd_idx) & 0xFFFF);
            }
        }

        if (!pre_index) {
            base = add ? base + offset : base - offset;
            regs.write(rn_idx, base);
        } else if (writeback) {
            regs.write(rn_idx, addr);
        }

        return (load && rd_idx == 15);
    }

    // Block Data Transfer (LDM / STM)
    template<typename R32, typename W32>
    static bool execute_block(uint32_t opcode, Registers& regs, R32 r32, W32 w32) {
        bool pre_index = (opcode >> 24) & 1;
        bool add = (opcode >> 23) & 1;
        bool psr_user = (opcode >> 22) & 1;
        bool writeback = (opcode >> 21) & 1;
        bool load = (opcode >> 20) & 1;
        uint8_t rn_idx = (opcode >> 16) & 0x0F;
        uint16_t reg_list = opcode & 0xFFFF;

        uint32_t base = regs.read(rn_idx);
        uint32_t count = 0;
        for (int i = 0; i < 16; ++i) {
            if ((reg_list >> i) & 1) count++;
        }

        uint32_t start_addr = base;
        if (!add) start_addr -= count * 4;
        if (pre_index == add) start_addr += 4;

        uint32_t current_addr = start_addr;
        bool pc_loaded = false;

        for (int i = 0; i < 16; ++i) {
            if ((reg_list >> i) & 1) {
                if (load) {
                    uint32_t val = r32(current_addr);
                    regs.write(i, val);
                    if (i == 15) pc_loaded = true;
                } else {
                    uint32_t val = regs.read(i);
                    if (i == 15) val += 12;
                    w32(current_addr, val);
                }
                current_addr += 4;
            }
        }

        if (writeback) {
            uint32_t new_base = add ? base + count * 4 : base - count * 4;
            regs.write(rn_idx, new_base);
        }

        if (load && pc_loaded && psr_user) {
            regs.restore_spsr();
        }

        return pc_loaded;
    }
};

} // namespace zGBA::CPU::ARM7TDMI::ARM