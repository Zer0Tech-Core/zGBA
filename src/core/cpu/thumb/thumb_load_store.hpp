#pragma once

#include <cstdint>
#include <functional>
#include "../arm7tdmi/registers.hpp"

namespace zGBA::CPU::ARM7TDMI::Thumb {

using Read8Func  = std::function<uint8_t(uint32_t)>;
using Read16Func = std::function<uint16_t(uint32_t)>;
using Read32Func = std::function<uint32_t(uint32_t)>;
using Write8Func  = std::function<void(uint32_t, uint8_t)>;
using Write16Func = std::function<void(uint32_t, uint16_t)>;
using Write32Func = std::function<void(uint32_t, uint32_t)>;

class ThumbLoadStore {
public:
    // Format 6: PC-Relative Load
    static void execute_ldr_pc(Registers& regs, uint16_t opcode, Read32Func read32) {
        uint8_t rd = (opcode >> 8) & 0x07;
        uint32_t imm = (opcode & 0xFF) << 2;
        //uint32_t addr = (regs.get_pc() & ~2) + imm;
        uint32_t addr = (regs.get_pc() & ~3) + imm;

        regs.write(rd, read32(addr));
    }

    // Format 7 & 8: Load/Store with Register Offset & Sign Extended Byte/Halfword
    static void execute_reg_offset(Registers& regs, uint16_t opcode, Read8Func read8, Read16Func read16, Read32Func read32, Write8Func write8, Write16Func write16, Write32Func write32) {
        uint8_t op = (opcode >> 9) & 0x07;
        uint8_t ro = (opcode >> 6) & 0x07;
        uint8_t rb = (opcode >> 3) & 0x07;
        uint8_t rd = opcode & 0x07;

        uint32_t addr = regs.read(rb) + regs.read(ro);

        switch (op) {
            case 0: write32(addr, regs.read(rd)); break;             // STR
            case 1: write16(addr, regs.read(rd)); break;             // STRH
            case 2: write8(addr, regs.read(rd)); break;              // STRB
            case 3: { // LDSB
                int8_t val = static_cast<int8_t>(read8(addr));
                regs.write(rd, static_cast<int32_t>(val));
                break;
            }
            case 4: regs.write(rd, read32(addr)); break;             // LDR
            case 5: regs.write(rd, read16(addr)); break;             // LDRH
            case 6: regs.write(rd, read8(addr)); break;              // LDRB
            case 7: { // LDSH
                int16_t val = static_cast<int16_t>(read16(addr));
                regs.write(rd, static_cast<int32_t>(val));
                break;
            }
        }
    }

    // Format 9: Load/Store Immediate Offset (Word / Byte)
    static void execute_imm_offset(Registers& regs, uint16_t opcode, Read8Func read8, Read32Func read32, Write8Func write8, Write32Func write32) {
        bool is_byte = (opcode >> 12) & 1;
        bool is_load = (opcode >> 11) & 1;
        uint8_t offset = (opcode >> 6) & 0x1F;
        uint8_t rb = (opcode >> 3) & 0x07;
        uint8_t rd = opcode & 0x07;

        uint32_t addr = regs.read(rb) + (is_byte ? offset : (offset << 2));

        if (is_load) {
            regs.write(rd, is_byte ? read8(addr) : read32(addr));
        } else {
            if (is_byte) write8(addr, static_cast<uint8_t>(regs.read(rd)));
            else write32(addr, regs.read(rd));
        }
    }

    // Format 10: Load/Store Halfword Immediate Offset
    static void execute_halfword_imm(Registers& regs, uint16_t opcode, Read16Func read16, Write16Func write16) {
        bool is_load = (opcode >> 11) & 1;
        uint8_t offset = (opcode >> 6) & 0x1F;
        uint8_t rb = (opcode >> 3) & 0x07;
        uint8_t rd = opcode & 0x07;

        uint32_t addr = regs.read(rb) + (offset << 1);

        if (is_load) {
            regs.write(rd, read16(addr));
        } else {
            write16(addr, static_cast<uint16_t>(regs.read(rd)));
        }
    }

    // Format 11: SP-Relative Load/Store
    static void execute_sp_relative(Registers& regs, uint16_t opcode, Read32Func read32, Write32Func write32) {
        bool is_load = (opcode >> 11) & 1;
        uint8_t rd = (opcode >> 8) & 0x07;
        uint32_t imm = (opcode & 0xFF) << 2;

        uint32_t addr = regs.read(13) + imm;

        if (is_load) {
            regs.write(rd, read32(addr));
        } else {
            write32(addr, regs.read(rd));
        }
    }

    // Format 14: PUSH / POP
    static void execute_push_pop(Registers& regs, uint16_t opcode, Read32Func read32, Write32Func write32) {
        bool is_pop = (opcode >> 11) & 1;
        bool p_lr_bit = (opcode >> 8) & 1;
        uint8_t rlist = opcode & 0xFF;

        uint32_t sp = regs.read(13);

        if (!is_pop) { // PUSH
            if (p_lr_bit) {
                sp -= 4;
                write32(sp, regs.read(14)); // Push LR
            }
            for (int i = 7; i >= 0; --i) {
                if ((rlist >> i) & 1) {
                    sp -= 4;
                    write32(sp, regs.read(i));
                }
            }
            regs.write(13, sp);
        } else { // POP
            for (int i = 0; i <= 7; ++i) {
                if ((rlist >> i) & 1) {
                    regs.write(i, read32(sp));
                    sp += 4;
                }
            }
            if (p_lr_bit) { // Pop para o PC
                regs.set_pc(read32(sp) & ~1U);
                sp += 4;
            }
            regs.write(13, sp);
        }
    }

    // Format 15: Multiple Load / Store (LDMIA / STMIA)
    static void execute_block_transfer(Registers& regs, uint16_t opcode, Read32Func read32, Write32Func write32) {
        bool is_load = (opcode >> 11) & 1;
        uint8_t rb = (opcode >> 8) & 0x07;
        uint8_t rlist = opcode & 0xFF;

        uint32_t addr = regs.read(rb);

        for (int i = 0; i <= 7; ++i) {
            if ((rlist >> i) & 1) {
                if (is_load) {
                    regs.write(i, read32(addr));
                } else {
                    write32(addr, regs.read(i));
                }
                addr += 4;
            }
        }
        regs.write(rb, addr);
    }
};

} // namespace zGBA::CPU::ARM7TDMI::Thumb