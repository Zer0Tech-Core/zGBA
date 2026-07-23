#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include <cstring>

namespace zGBA::CPU::ARM7TDMI {

enum class Mode : uint8_t {
    User       = 0x10,
    FIQ        = 0x11,
    IRQ        = 0x12,
    Supervisor = 0x13,
    Abort      = 0x17,
    Undefined  = 0x1B,
    System     = 0x1F
};

enum class BankIndex : uint8_t {
    UserSys = 0,
    FIQ     = 1,
    IRQ     = 2,
    SVC     = 3,
    ABT     = 4,
    UND     = 5,
    Invalid = 0xFF
};

class Registers {
public:
    enum Flag : uint32_t {
        N = 1u << 31,
        Z = 1u << 30,
        C = 1u << 29,
        V = 1u << 28,
        I = 1u << 7,
        F = 1u << 6,
        T = 1u << 5
    };

    alignas(16) uint32_t r[16]{0};
    uint32_t cpsr{0x000000D3};

    void reset();

    [[nodiscard]] inline uint32_t read(size_t reg) const { return r[reg]; }
    inline void write(size_t reg, uint32_t val) { r[reg] = val; }

    [[nodiscard]] inline uint32_t get_pc() const { return r[15]; }
    inline void set_pc(uint32_t val) { r[15] = val; }

    void set_mode(Mode new_mode);
    [[nodiscard]] Mode get_mode() const;

    [[nodiscard]] inline bool is_flag_set(Flag flag) const { return (cpsr & flag) != 0; }
    inline void set_flag(Flag flag, bool value) {
        if (value) cpsr |= flag;
        else cpsr &= ~flag;
    }

    [[nodiscard]] inline bool is_thumb() const { return is_flag_set(Flag::T); }

    [[nodiscard]] uint32_t read_spsr() const;
    void write_spsr(uint32_t val);

private:
    uint32_t fiq_r8_14[7]{0};
    uint32_t usr_r8_12[5]{0};
    uint32_t r13_14_bank[6][2]{0};
    uint32_t spsr_bank[6]{0};

    static BankIndex get_bank_index(Mode mode);
};

} // namespace zGBA::CPU::ARM7TDMI