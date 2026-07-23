#include "memory_map.hpp"
#include <fstream>
#include <bit>
#include <algorithm>

namespace zgba::memory {

MemoryMap::MemoryMap() {
    bios.fill(0x00);
    ewram.fill(0x00);
    iwram.fill(0x00);
    io_regs.fill(0x00);
    palette_ram.fill(0x00);
    vram.fill(0x00);
    oam.fill(0x00);
    sram.fill(0xFF);
}

bool MemoryMap::load_bios(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;

    const auto size = file.tellg();
    if (size != BIOS_SIZE) return false;

    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(bios.data()), BIOS_SIZE);
    return true;
}

bool MemoryMap::load_rom(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;

    const auto size = static_cast<std::size_t>(file.tellg());
    if (size == 0 || size > 32 * 1024 * 1024) return false;

    gamepak_rom.resize(size);
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(gamepak_rom.data()), size);

    // Ajusta a máscara da ROM para a próxima potência de 2
    uint32_t pow2_size = std::bit_ceil(static_cast<uint32_t>(size));
    rom_mask = pow2_size - 1;

    return true;
}

bool MemoryMap::load_bios_from_memory(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return false;
    }

    size_t copy_size = std::min(data.size(), BIOS_SIZE);
    std::copy_n(data.begin(), copy_size, bios.begin());
    return true;
}

} // namespace zgba::memory