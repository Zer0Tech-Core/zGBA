#include "rom_loader.hpp"
#include "../core/embedded_bios.hpp"
#include <iostream>
#include <vector>

bool RomLoader::loadGamePak(const std::string& filepath, zgba::memory::MemoryMap& memory_map) {
    if (!memory_map.load_rom(filepath)) {
        std::cerr << "Erro: Não foi possível carregar a ROM do Game Pak: " << filepath << std::endl;
        return false;
    }
    return true;
}

bool RomLoader::loadBios(const std::string& filepath, zgba::memory::MemoryMap& memory_map) {
    if (!memory_map.load_bios(filepath)) {
        std::cerr << "Aviso: Falha ao carregar a BIOS: " << filepath << std::endl;
        return false;
    }
    return true;
}

bool RomLoader::loadEmbeddedBios(zgba::memory::MemoryMap& memory_map) {
    std::vector<uint8_t> bios_data(bios_bin, bios_bin + bios_bin_len);
    if (!memory_map.load_bios_from_memory(bios_data)) {
        std::cerr << "Erro: Falha ao copiar a BIOS embutida para o mapa de memória!" << std::endl;
        return false;
    }
    std::cout << "BIOS embutida carregada com sucesso (" << bios_bin_len << " bytes)." << std::endl;
    return true;
}