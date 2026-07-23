#include "registers.hpp"

namespace zGBA::CPU::ARM7TDMI {

BankIndex Registers::get_bank_index(Mode mode) {
    switch (mode) {
        case Mode::User:
        case Mode::System:     return BankIndex::UserSys;
        case Mode::FIQ:        return BankIndex::FIQ;
        case Mode::IRQ:        return BankIndex::IRQ;
        case Mode::Supervisor: return BankIndex::SVC;
        case Mode::Abort:      return BankIndex::ABT;
        case Mode::Undefined:  return BankIndex::UND;
        default:               return BankIndex::Invalid;
    }
}

void Registers::reset() {
    std::memset(r, 0, sizeof(r));
    std::memset(fiq_r8_14, 0, sizeof(fiq_r8_14));
    std::memset(usr_r8_12, 0, sizeof(usr_r8_12));
    std::memset(r13_14_bank, 0, sizeof(r13_14_bank));
    std::memset(spsr_bank, 0, sizeof(spsr_bank));

    cpsr = 0x000000D3; 
    r[15] = 0x00000000;
}

void Registers::set_mode(Mode new_mode) {
    Mode old_mode = get_mode();
    if (old_mode == new_mode) return;

    BankIndex old_bank = get_bank_index(old_mode);
    BankIndex new_bank = get_bank_index(new_mode);

    r13_14_bank[static_cast<size_t>(old_bank)][0] = r[13];
    r13_14_bank[static_cast<size_t>(old_bank)][1] = r[14];

    if (old_mode == Mode::FIQ) {
        std::memcpy(fiq_r8_14, &r[8], sizeof(uint32_t) * 7);
        std::memcpy(&r[8], usr_r8_12, sizeof(uint32_t) * 5);
    } else if (new_mode == Mode::FIQ) {
        std::memcpy(usr_r8_12, &r[8], sizeof(uint32_t) * 5);
        std::memcpy(&r[8], fiq_r8_14, sizeof(uint32_t) * 7);
    }

    if (new_mode != Mode::FIQ) {
        r[13] = r13_14_bank[static_cast<size_t>(new_bank)][0];
        r[14] = r13_14_bank[static_cast<size_t>(new_bank)][1];
    }

    cpsr = (cpsr & ~0x1Fu) | static_cast<uint32_t>(new_mode);
}

Mode Registers::get_mode() const {
    return static_cast<Mode>(cpsr & 0x1Fu);
}

uint32_t Registers::read_spsr() const {
    BankIndex bank = get_bank_index(get_mode());
    return spsr_bank[static_cast<size_t>(bank)];
}

void Registers::write_spsr(uint32_t val) {
    BankIndex bank = get_bank_index(get_mode());
    if (bank != BankIndex::UserSys) {
        spsr_bank[static_cast<size_t>(bank)] = val;
    }
}

} // namespace zGBA::CPU::ARM7TDMI