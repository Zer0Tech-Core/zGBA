#pragma once

#include <cstdint>
#include <vector>
#include <array>
#include <string_view>
#include <filesystem>

namespace zgba::memory {

// Tamanhos fixos do hardware do GBA
constexpr std::size_t BIOS_SIZE    = 16 * 1024;        // 16 KB
constexpr std::size_t EWRAM_SIZE   = 256 * 1024;       // 256 KB
constexpr std::size_t IWRAM_SIZE   = 32 * 1024;        // 32 KB
constexpr std::size_t IO_SIZE      = 1 * 1024;         // 1 KB
constexpr std::size_t PALETTE_SIZE = 1 * 1024;         // 1 KB
constexpr std::size_t VRAM_SIZE    = 96 * 1024;        // 96 KB
constexpr std::size_t OAM_SIZE     = 1 * 1024;         // 1 KB
constexpr std::size_t SRAM_SIZE    = 64 * 1024;        // 64 KB (Flash/SRAM)

// Endereços Base das Regiões
constexpr uint32_t ADDR_BIOS    = 0x00000000;
constexpr uint32_t ADDR_EWRAM   = 0x02000000;
constexpr uint32_t ADDR_IWRAM   = 0x03000000;
constexpr uint32_t ADDR_IO      = 0x04000000;
constexpr uint32_t ADDR_PALETTE = 0x05000000;
constexpr uint32_t ADDR_VRAM    = 0x06000000;
constexpr uint32_t ADDR_OAM     = 0x07000000;
constexpr uint32_t ADDR_GAMEPAK = 0x08000000;
constexpr uint32_t ADDR_SRAM    = 0x0E000000;

class MemoryMap {
public:
    MemoryMap();
    ~MemoryMap() = default;

    bool load_bios(const std::filesystem::path& path);
    bool load_rom(const std::filesystem::path& path);
    bool load_bios_from_memory(const std::vector<uint8_t>& data);

    // Buffers de memória
    std::array<uint8_t, BIOS_SIZE> bios{};
    std::array<uint8_t, EWRAM_SIZE> ewram{};
    std::array<uint8_t, IWRAM_SIZE> iwram{};
    std::array<uint8_t, IO_SIZE> io_regs{};
    std::array<uint8_t, PALETTE_SIZE> palette_ram{};
    std::array<uint8_t, VRAM_SIZE> vram{};
    std::array<uint8_t, OAM_SIZE> oam{};
    std::array<uint8_t, SRAM_SIZE> sram{};
    
    std::vector<uint8_t> gamepak_rom{};
    uint32_t rom_mask{0};

    // Auxiliar para espelhamento do VRAM (Blocos de 64KB + 32KB em área de 128KB)
    [[nodiscard]] static uint32_t mirror_vram_addr(uint32_t addr) noexcept {
        uint32_t offset = addr & 0x1FFFF;
        if (offset >= VRAM_SIZE) {
            offset -= 0x8000; // Espelhamento do bloco superior de 32KB
        }
        return offset;
    }
};

} // namespace zgba::memory