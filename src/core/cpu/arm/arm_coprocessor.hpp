#pragma once

#include <cstdint>
#include "../arm7tdmi/registers.hpp"
#include "../arm7tdmi/exceptions.hpp"

namespace zGBA::CPU::ARM7TDMI::ARM {

class ARMCoprocessor {
public:
    static bool execute(uint32_t /*opcode*/, Registers& regs) {
        // No GBA, coprocessadores não são mapeados e disparam Undefined Instruction Exception
        Exceptions::raise(regs, ExceptionType::UndefinedInstruction);
        return true; // Flush por alteração de PC devido à exceção
    }
};

} // namespace zGBA::CPU::ARM7TDMI::ARM