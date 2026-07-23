#pragma once
#include <string>
#include "../core/memory/memory_map.hpp"

class RomLoader {
public:
    RomLoader() = default;
    ~RomLoader() = default;

    bool loadGamePak(const std::string& filepath, zgba::memory::MemoryMap& memory_map);
    bool loadBios(const std::string& filepath, zgba::memory::MemoryMap& memory_map);
    bool loadEmbeddedBios(zgba::memory::MemoryMap& memory_map);
};