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

class ThumbDecoder {
public:
    static bool decode_and_execute(
        uint16_t opcode,
        Registers& regs,
        Read8Func read8,
        Read16Func read16,
        Read32Func read32,
        Write8Func write8,
        Write16Func write16,
        Write32Func write32
    );
};

} // namespace zGBA::CPU::ARM7TDMI::Thumb