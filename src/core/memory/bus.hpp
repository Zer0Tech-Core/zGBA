#pragma once

#include "memory_map.hpp"
#include <cstdint>

namespace zgba::memory {

class Bus {
public:
    explicit Bus(MemoryMap& memory_map);

    // Leitura e Escrita genéricas
    [[nodiscard]] uint8_t  read8(uint32_t addr, uint32_t current_pc = 0xFFFFFFFF);
    [[nodiscard]] uint16_t read16(uint32_t addr, uint32_t current_pc = 0xFFFFFFFF);
    [[nodiscard]] uint32_t read32(uint32_t addr, uint32_t current_pc = 0xFFFFFFFF);

    void write8(uint32_t addr, uint8_t val);
    void write16(uint32_t addr, uint16_t val);
    void write32(uint32_t addr, uint32_t val);
    
    void request_interrupt(uint16_t interrupt_mask);

    // Acúmulo e recuperação de ciclos de memória (Wait States)
    [[nodiscard]] uint64_t consume_cycles() noexcept {
        uint64_t c = pending_cycles;
        pending_cycles = 0;
        return c;
    }

    // Registrador Open Bus (Guarda último valor lido do barramento)
    uint32_t last_bus_value{0};

private:
    MemoryMap& mem;
    uint64_t pending_cycles{0};

    void calculate_access_cycles(uint32_t addr, uint32_t access_size, bool sequential = false);
    void write16_internal(uint32_t addr, uint16_t val); // Adicionado para evitar contagem dupla de ciclos
    
    [[nodiscard]] bool is_bios_locked(uint32_t pc) const noexcept {
        return pc >= 0x00004000;
    }
};

} // namespace zgba::memory