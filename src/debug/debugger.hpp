#pragma once
#include <cstdint>
#include <unordered_set>
#include "../core/cpu/cpu.hpp"

namespace zgba::debug {

class Debugger {
public:
    explicit Debugger(zGBA::CPU::ARM7TDMI::CPU& cpu);
    ~Debugger() = default;

    void add_breakpoint(uint32_t address);
    void remove_breakpoint(uint32_t address);
    bool check_breakpoint(uint32_t address) const;

    void set_enabled(bool enabled);
    bool is_enabled() const;

    void update();
    void print_state() const;
    void interactive_prompt(); // Novo método para a CLI

private:
    zGBA::CPU::ARM7TDMI::CPU& cpu_ref;
    std::unordered_set<uint32_t> breakpoints;
    bool enabled;
};

} // namespace zgba::debug