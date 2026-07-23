#pragma once
#include <string>
#include <cstdint>
#include "rom_loader.hpp"
#include "input.hpp"
#include "renderer.hpp"
#include "audio_backend.hpp"
#include "../core/memory/memory_map.hpp"
#include "../core/memory/bus.hpp"
#include "../core/cpu/cpu.hpp"
#include "../core/video/ppu.hpp"
#include "../debug/debugger.hpp"

class Emulador {
public:
    Emulador();
    ~Emulador() = default;

    bool init(const std::string& rom_path);
    void run();

    void enable_debugger(bool enable, uint32_t initial_breakpoint = 0x08000000) {
        debugger.set_enabled(enable);
        if (enable) {
            debugger.add_breakpoint(initial_breakpoint);
        }
    }

private:
    void step();

    bool is_running;
    
    static constexpr int CYCLES_PER_SCANLINE = 1232;
    static constexpr int SCANLINES_PER_FRAME = 228;
    static constexpr int CYCLES_PER_FRAME = CYCLES_PER_SCANLINE * SCANLINES_PER_FRAME; 

    RomLoader loader;
    Input input;
    Renderer renderer;
    AudioBackend audio;

    zgba::memory::MemoryMap memory_map;
    zgba::memory::Bus memory_bus{memory_map};
    zGBA::CPU::ARM7TDMI::CPU cpu;
    zgba::video::PPU ppu;
    
    zgba::debug::Debugger debugger{cpu}; // Adicionado aqui

    uint32_t framebuffer[240 * 160];
};