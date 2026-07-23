#include "thumb_decoder.hpp"
#include "thumb_alu.hpp"
#include "thumb_branch.hpp"
#include "thumb_load_store.hpp"
#include "../arm7tdmi/exceptions.hpp"

namespace zGBA::CPU::ARM7TDMI::Thumb {

bool ThumbDecoder::decode_and_execute(
    uint16_t opcode,
    Registers& regs,
    Read8Func read8,
    Read16Func read16,
    Read32Func read32,
    Write8Func write8,
    Write16Func write16,
    Write32Func write32
) {
    // Format 1: Move Shifted Register
    if ((opcode & 0xE000) == 0x0000 && (opcode & 0x1800) != 0x1800) {
        ThumbALU::execute_shift_imm(regs, opcode);
        return false;
    }

    // Format 2: Add / Subtract
    if ((opcode & 0xF800) == 0x1800) {
        ThumbALU::execute_add_sub(regs, opcode);
        return false;
    }

    // Format 3: Move/Compare/Add/Subtract Immediate
    if ((opcode & 0xE000) == 0x2000) {
        ThumbALU::execute_imm_op(regs, opcode);
        return false;
    }

    // Format 4: ALU Operations
    if ((opcode & 0xFC00) == 0x4000) {
        ThumbALU::execute_alu_ops(regs, opcode);
        return false;
    }

    // Format 5: Hi Register Ops / BX
    if ((opcode & 0xFC00) == 0x4400) {
        uint8_t op = (opcode >> 8) & 0x03;
        if (op == 3) { // BX
            return ThumbBranch::execute_bx(regs, opcode);
        } else {
            ThumbALU::execute_hi_reg_op(regs, opcode);
            return false;
        }
    }

    // Format 6: PC-Relative Load
    if ((opcode & 0xF800) == 0x4800) {
        ThumbLoadStore::execute_ldr_pc(regs, opcode, read32);
        return false;
    }

    // Format 7 & 8: Load/Store Register Offset / Sign Extended
    if ((opcode & 0xF200) == 0x5000 || (opcode & 0xF200) == 0x5200) {
        ThumbLoadStore::execute_reg_offset(regs, opcode, read8, read16, read32, write8, write16, write32);
        return false;
    }

    // Format 9: Load/Store Immediate Offset
    if ((opcode & 0xE000) == 0x6000) {
        ThumbLoadStore::execute_imm_offset(regs, opcode, read8, read32, write8, write32);
        return false;
    }

    // Format 10: Load/Store Halfword
    if ((opcode & 0xF000) == 0x8000) {
        ThumbLoadStore::execute_halfword_imm(regs, opcode, read16, write16);
        return false;
    }

    // Format 11: SP-Relative Load/Store
    if ((opcode & 0xF000) == 0x9000) {
        ThumbLoadStore::execute_sp_relative(regs, opcode, read32, write32);
        return false;
    }

    // Format 12: Load Address (ADD Rd, PC/SP, #imm)
    if ((opcode & 0xF000) == 0xA000) {
        ThumbALU::execute_add_sp_pc(regs, opcode);
        return false;
    }

    // Format 13: Add Offset to SP
    if ((opcode & 0xFF00) == 0xB000) {
        ThumbALU::execute_adjust_sp(regs, opcode);
        return false;
    }

    // Format 14: Push / Pop Registers
    if ((opcode & 0xF600) == 0xB400) {
        ThumbLoadStore::execute_push_pop(regs, opcode, read32, write32);
        return false;
    }

    // Format 15: Multiple Load / Store
    if ((opcode & 0xF000) == 0xC000) {
        ThumbLoadStore::execute_block_transfer(regs, opcode, read32, write32);
        return false;
    }

    // Format 17: Software Interrupt (SWI)
    if ((opcode & 0xFF00) == 0xDF00) {
        Exceptions::raise(regs, ExceptionType::SoftwareInterrupt);
        return true;
    }

    // Format 16: Conditional Branch
    if ((opcode & 0xF000) == 0xD000) {
        return ThumbBranch::execute_cond_branch(regs, opcode);
    }

    // Format 18: Unconditional Branch
    if ((opcode & 0xF800) == 0xE000) {
        ThumbBranch::execute_uncond_branch(regs, opcode);
        return true;
    }

    // Format 19: Long Branch with Link (BL)
    if ((opcode & 0xF000) == 0xF000) {
        bool is_suffix = (opcode >> 11) & 1;
        if (!is_suffix) {
            ThumbBranch::execute_bl_prefix(regs, opcode);
            return false;
        } else {
            ThumbBranch::execute_bl_suffix(regs, opcode);
            return true;
        }
    }

    // Instrução não reconhecida
    Exceptions::raise(regs, ExceptionType::UndefinedInstruction);
    return true;
}

} // namespace zGBA::CPU::ARM7TDMI::Thumb