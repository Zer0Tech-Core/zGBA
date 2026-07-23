#include "emulador.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <algorithm>

Emulador::Emulador() : is_running(false) {
    std::fill(std::begin(framebuffer), std::end(framebuffer), 0xFF000000);

    zGBA::CPU::ARM7TDMI::CPU::MemoryBus cpu_bus_callbacks{
        [this](uint32_t addr) { return memory_bus.read8(addr, cpu.get_registers().get_pc()); },
        [this](uint32_t addr) { return memory_bus.read16(addr, cpu.get_registers().get_pc()); },
        [this](uint32_t addr) { return memory_bus.read32(addr, cpu.get_registers().get_pc()); },
        [this](uint32_t addr, uint8_t val) { memory_bus.write8(addr, val); },
        [this](uint32_t addr, uint16_t val) { memory_bus.write16(addr, val); },
        [this](uint32_t addr, uint32_t val) { memory_bus.write32(addr, val); }
    };
    cpu.set_bus(cpu_bus_callbacks);
}

bool Emulador::init(const std::string& rom_path) {
    if (!renderer.init("zGBA - Game Boy Advance Emulator", 3)) {
        return false;
    }

    // Carrega a BIOS embutida diretamente
    if (!loader.loadEmbeddedBios(memory_map)) {
        std::cerr << "Erro fatal: Falha ao carregar a BIOS embutida." << std::endl;
        return false;
    }

    if (!loader.loadGamePak(rom_path, memory_map)) {
        std::cerr << "Erro fatal: Falha ao carregar a ROM do Game Pak." << std::endl;
        return false;
    }

    cpu.reset();
    is_running = true;
    return true;
}

void Emulador::run() {
    is_running = true;
    const auto frame_duration = std::chrono::nanoseconds(16742706);

    while (is_running) {
        auto frame_start = std::chrono::high_resolution_clock::now();
        input.pollEvents(is_running);

        uint32_t frame_cycles = 0;
        while (frame_cycles < CYCLES_PER_FRAME && is_running) {
            debugger.update();
            cpu.step();
            uint64_t cycles_spent = memory_bus.consume_cycles();
            if (cycles_spent == 0) cycles_spent = 1;
            frame_cycles += static_cast<uint32_t>(cycles_spent);
            ppu.step(static_cast<uint32_t>(cycles_spent), memory_bus);
        }

        const auto& ppu_fb = ppu.get_framebuffer();
        std::copy(ppu_fb.begin(), ppu_fb.end(), std::begin(framebuffer));

        renderer.renderFrame(framebuffer);

        // resto do controle de timing (spin-lock) permanece igual
        auto frame_end = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(frame_end - frame_start);
        if (elapsed < frame_duration) {
            auto sleep_time = frame_duration - elapsed;
            if (sleep_time > std::chrono::milliseconds(1)) {
                std::this_thread::sleep_for(sleep_time - std::chrono::milliseconds(1));
            }
            while (std::chrono::high_resolution_clock::now() - frame_start < frame_duration) {}
        }
    }
}

void Emulador::step() {
    debugger.update();
    cpu.step();
    uint64_t cycles_spent = memory_bus.consume_cycles();
    if (cycles_spent == 0) cycles_spent = 1;
    ppu.step(static_cast<uint32_t>(cycles_spent), memory_bus);
}
