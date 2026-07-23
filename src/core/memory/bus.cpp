#include "bus.hpp"
#include <cstring>

namespace zgba::memory {

Bus::Bus(MemoryMap& memory_map) : mem(memory_map) {}

// ============================================================================
// LEITURA DE MEMÓRIA (READ)
// ============================================================================

uint8_t Bus::read8(uint32_t addr, uint32_t current_pc) {
    calculate_access_cycles(addr, 8);
    const uint8_t region = (addr >> 24) & 0xFF;

    switch (region) {
        case 0x00: // BIOS (32-bit bus)[cite: 50]
            if (addr < BIOS_SIZE) {
                if (is_bios_locked(current_pc)) {
                    return static_cast<uint8_t>(last_bus_value >> ((addr & 3) * 8));
                }
                return mem.bios[addr];
            }
            break;

        case 0x02: // EWRAM (16-bit bus, 256 KB)[cite: 50, 51]
            return mem.ewram[addr & (EWRAM_SIZE - 1)];

        case 0x03: // IWRAM (32-bit bus, 32 KB)[cite: 50, 51]
            return mem.iwram[addr & (IWRAM_SIZE - 1)];

        case 0x04: // I/O Registers (32-bit bus, 1 KB)[cite: 50, 51]
            if ((addr & 0x00FFFFFF) < IO_SIZE) {
                return mem.io_regs[addr & (IO_SIZE - 1)];
            }
            break;

        case 0x05: // Palette RAM (16-bit bus)[cite: 50, 51]
            return mem.palette_ram[addr & (PALETTE_SIZE - 1)];

        case 0x06: // VRAM (16-bit bus)[cite: 50, 51]
            return mem.vram[MemoryMap::mirror_vram_addr(addr)];

        case 0x07: // OAM (32-bit bus)[cite: 50, 51]
            return mem.oam[addr & (OAM_SIZE - 1)];

        case 0x08: case 0x09: // Game Pak ROM (WS0)[cite: 50, 51]
        case 0x0A: case 0x0B: // Game Pak ROM (WS1)[cite: 50, 51]
        case 0x0C: case 0x0D: // Game Pak ROM (WS2)[cite: 50, 51]
            {
                uint32_t rom_addr = addr & mem.rom_mask;
                if (rom_addr < mem.gamepak_rom.size()) {
                    last_bus_value = mem.gamepak_rom[rom_addr]; // Simplificado para open bus
                    return mem.gamepak_rom[rom_addr];
                }
            }
            break;

        case 0x0E: case 0x0F: // Cart RAM / SRAM (8-bit bus)[cite: 50, 51]
            {
                uint8_t val = mem.sram[addr & (SRAM_SIZE - 1)];
                last_bus_value = (val << 16) | val; // Ajuste open bus SRAM
                return val;
            }

        default:
            break;
    }

    return static_cast<uint8_t>(last_bus_value >> ((addr & 3) * 8));
}

uint16_t Bus::read16(uint32_t addr, uint32_t current_pc) {
    addr &= ~1U; // Alinhamento de 16 bits
    calculate_access_cycles(addr, 16);

    const uint8_t region = (addr >> 24) & 0xFF;
    uint16_t val = 0;
    uint32_t offset = addr & 0x00FFFFFF;

    switch (region) {
        case 0x00: // BIOS
            if (offset < BIOS_SIZE) {
                if (is_bios_locked(current_pc)) return static_cast<uint16_t>(last_bus_value);
                val = *reinterpret_cast<const uint16_t*>(&mem.bios[offset]);
            }
            break;

        case 0x02: // EWRAM
            val = *reinterpret_cast<const uint16_t*>(&mem.ewram[offset & (EWRAM_SIZE - 1)]);
            break;

        case 0x03: // IWRAM
            val = *reinterpret_cast<const uint16_t*>(&mem.iwram[offset & (IWRAM_SIZE - 1)]);
            break;

        case 0x04: // I/O Registers
            if (offset < IO_SIZE) {
                val = *reinterpret_cast<const uint16_t*>(&mem.io_regs[offset & (IO_SIZE - 1)]);
            }
            break;

        case 0x05: // Palette RAM
            val = *reinterpret_cast<const uint16_t*>(&mem.palette_ram[offset & (PALETTE_SIZE - 1)]);
            break;

        case 0x06: // VRAM
            val = *reinterpret_cast<const uint16_t*>(&mem.vram[MemoryMap::mirror_vram_addr(addr)]);
            break;

        case 0x07: // OAM
            val = *reinterpret_cast<const uint16_t*>(&mem.oam[offset & (OAM_SIZE - 1)]);
            break;

        case 0x08: case 0x09:
        case 0x0A: case 0x0B:
        case 0x0C: case 0x0D: // Game Pak ROM
            {
                uint32_t rom_addr = addr & mem.rom_mask;
                if (rom_addr + 1 < mem.gamepak_rom.size()) {
                    val = *reinterpret_cast<const uint16_t*>(&mem.gamepak_rom[rom_addr]);
                } else if (rom_addr < mem.gamepak_rom.size()) {
                    val = mem.gamepak_rom[rom_addr];
                }
            }
            break;

        case 0x0E: case 0x0F: // SRAM (Acessado como 16-bit em alguns contextos, lê duplicado ou byte)
            {
                uint8_t b = mem.sram[offset & (SRAM_SIZE - 1)];
                val = static_cast<uint16_t>(b | (b << 8));
            }
            break;

        default:
            val = static_cast<uint16_t>(last_bus_value);
            break;
    }

    last_bus_value = (last_bus_value & 0xFFFF0000) | val;
    return val;
}

uint32_t Bus::read32(uint32_t addr, uint32_t current_pc) {
    addr &= ~3U; // Alinhamento para 32-bit
    calculate_access_cycles(addr, 32);

    const uint8_t region = (addr >> 24) & 0xFF;
    uint32_t val = 0;
    uint32_t offset = addr & 0x00FFFFFF;

    switch (region) {
        case 0x00: // BIOS[cite: 50, 51]
            if (offset < BIOS_SIZE) {
                if (is_bios_locked(current_pc)) return last_bus_value;
                val = *reinterpret_cast<const uint32_t*>(&mem.bios[offset]);
            }
            break;

        case 0x02: // EWRAM[cite: 50, 51]
            val = *reinterpret_cast<const uint32_t*>(&mem.ewram[offset & (EWRAM_SIZE - 1)]);
            break;

        case 0x03: // IWRAM[cite: 50, 51]
            val = *reinterpret_cast<const uint32_t*>(&mem.iwram[offset & (IWRAM_SIZE - 1)]);
            break;

        case 0x04: // I/O Registers[cite: 50, 51]
            if (offset < IO_SIZE) {
                val = *reinterpret_cast<const uint32_t*>(&mem.io_regs[offset & (IO_SIZE - 1)]);
            }
            break;

        case 0x05: // Palette RAM[cite: 50, 51]
            val = *reinterpret_cast<const uint32_t*>(&mem.palette_ram[offset & (PALETTE_SIZE - 1)]);
            break;

        case 0x06: // VRAM
            {
                uint32_t low_addr = MemoryMap::mirror_vram_addr(addr);
                uint32_t high_addr = MemoryMap::mirror_vram_addr(addr + 2);
                
                uint16_t low_val  = *reinterpret_cast<const uint16_t*>(&mem.vram[low_addr]);
                uint16_t high_val = *reinterpret_cast<const uint16_t*>(&mem.vram[high_addr]);
                
                val = low_val | (static_cast<uint32_t>(high_val) << 16);
            }
            break;

        case 0x07: // OAM[cite: 50, 51]
            val = *reinterpret_cast<const uint32_t*>(&mem.oam[offset & (OAM_SIZE - 1)]);
            break;

        case 0x08: case 0x09:
        case 0x0A: case 0x0B:
        case 0x0C: case 0x0D: // Game Pak ROM (Lê 32-bit como duas leituras de 16-bit ou direto se alinhado)[cite: 50, 51]
            {
                uint32_t rom_addr = addr & mem.rom_mask;
                if (rom_addr + 3 < mem.gamepak_rom.size()) {
                    val = *reinterpret_cast<const uint32_t*>(&mem.gamepak_rom[rom_addr]);
                }
            }
            break;

        default:
            val = last_bus_value;
            break;
    }

    last_bus_value = val;
    return val;
}

void Bus::request_interrupt(uint16_t interrupt_mask) {
    uint16_t current_if = read16(0x04000202);
    write16(0x04000202, current_if | interrupt_mask);
}

// ============================================================================
// ESCRITA DE MEMÓRIA (WRITE)
// ============================================================================

void Bus::write8(uint32_t addr, uint8_t val) {
    calculate_access_cycles(addr, 8);
    const uint8_t region = (addr >> 24) & 0xFF;
    uint32_t offset = addr & 0x00FFFFFF;

    switch (region) {
        case 0x02: // EWRAM
            mem.ewram[offset & (EWRAM_SIZE - 1)] = val;
            break;

        case 0x03: // IWRAM
            mem.iwram[offset & (IWRAM_SIZE - 1)] = val;
            break;

        case 0x04: // I/O Registers
            if (offset < IO_SIZE) {
                mem.io_regs[offset & (IO_SIZE - 1)] = val;
            }
            break;

        case 0x05: case 0x06: // Palette e VRAM (Escrita de 8-bit duplica o byte em 16-bit)[cite: 47]
            {
                uint16_t val16 = static_cast<uint16_t>(val | (val << 8));
                write16_internal(addr & ~1U, val16);
            }
            break;

        case 0x07: // OAM (Escritas de 8 bits ignoradas)[cite: 47]
            break;

        case 0x0E: case 0x0F: // SRAM
            mem.sram[offset & (SRAM_SIZE - 1)] = val;
            break;

        default:
            break;
    }
}

void Bus::write16(uint32_t addr, uint16_t val) {
    addr &= ~1U;
    calculate_access_cycles(addr, 16);
    write16_internal(addr, val);
}

// Método auxiliar interno para escrita de 16-bit sem duplicar o cálculo de ciclos
void Bus::write16_internal(uint32_t addr, uint16_t val) {
    const uint8_t region = (addr >> 24) & 0xFF;
    uint32_t offset = addr & 0x00FFFFFF;

    switch (region) {
        case 0x02: // EWRAM
            *reinterpret_cast<uint16_t*>(&mem.ewram[offset & (EWRAM_SIZE - 1)]) = val;
            break;

        case 0x03: // IWRAM
            *reinterpret_cast<uint16_t*>(&mem.iwram[offset & (IWRAM_SIZE - 1)]) = val;
            break;

        case 0x04: // I/O Registers
            if (offset < IO_SIZE) {
                *reinterpret_cast<uint16_t*>(&mem.io_regs[offset & (IO_SIZE - 1)]) = val;
            }
            break;

        case 0x05: // Palette RAM
            *reinterpret_cast<uint16_t*>(&mem.palette_ram[offset & (PALETTE_SIZE - 1)]) = val;
            break;

        case 0x06: // VRAM
            *reinterpret_cast<uint16_t*>(&mem.vram[MemoryMap::mirror_vram_addr(addr)]) = val;
            break;

        case 0x07: // OAM
            *reinterpret_cast<uint16_t*>(&mem.oam[offset & (OAM_SIZE - 1)]) = val;
            break;

        default:
            break;
    }
}

void Bus::write32(uint32_t addr, uint32_t val) {
    addr &= ~3U;
    calculate_access_cycles(addr, 32); // Conta ciclos corretos de 32 bits uma única vez
    
    // Escreve as duas halves usando o método interno sem disparar nova contagem de ciclos
    write16_internal(addr, static_cast<uint16_t>(val & 0xFFFF));
    write16_internal(addr + 2, static_cast<uint16_t>(val >> 16));
}

// ============================================================================
// CÁLCULO DE CICLOS DE WAIT STATES
// ============================================================================

void Bus::calculate_access_cycles(uint32_t addr, uint32_t access_size, bool sequential) {
    const uint8_t region = (addr >> 24) & 0xFF;
    uint32_t cycles = 1;

    switch (region) {
        case 0x02: // EWRAM (16-bit bus: 2 ciclos por acesso de 16-bit)[cite: 50]
            cycles = (access_size == 32) ? 4 : 2;
            break;

        case 0x03: case 0x00: case 0x04: // IWRAM, BIOS, I/O (32-bit bus: 1 ciclo)[cite: 50]
            cycles = 1;
            break;

        case 0x05: case 0x06: // Palette RAM & VRAM[cite: 50]
            cycles = (access_size == 32) ? 2 : 1;
            break;

        case 0x07: // OAM[cite: 50]
            cycles = 1;
            break;

        case 0x08: case 0x09: case 0x0A: case 0x0B: case 0x0C: case 0x0D: // Game Pak ROM[cite: 50]
            {
                uint16_t waitcnt = *reinterpret_cast<uint16_t*>(&mem.io_regs[0x0204]);
                static const uint8_t init_cycles[] = {4, 3, 2, 8};
                uint8_t ws0_first = init_cycles[(waitcnt >> 2) & 0x03];
                uint8_t ws0_seq   = ((waitcnt >> 4) & 1) ? 1 : 2;

                cycles = sequential ? ws0_seq : ws0_first;
                if (access_size == 32) cycles += ws0_seq;
            }
            break;

        case 0x0E: case 0x0F: // SRAM (8-bit bus, 4 ciclos padrão)[cite: 50]
            cycles = 4;
            break;

        default:
            cycles = 1;
            break;
    }

    pending_cycles += cycles;
}

} // namespace zgba::memory