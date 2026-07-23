#ifndef ZGBA_VIDEO_PPU_HPP
#define ZGBA_VIDEO_PPU_HPP

#include "video_types.hpp"
#include <array>
#include <cstdint>
#include <vector>

namespace zgba::memory {
class Bus;
}

namespace zgba::video {

class PPU {
public:
    PPU();
    ~PPU() = default;

    void reset();

    // Avança o timing do PPU baseado nos ciclos executados pela CPU
    void step(uint32_t cpu_cycles, memory::Bus& bus);

    // Leitura e Escrita de Registradores de Vídeo (Mapeados em 0x04000000)
    uint16_t read_register(uint32_t addr) const;
    void write_register(uint32_t addr, uint16_t value);

    // Acesso às Memórias Dedicadas
    uint8_t* get_vram() { return vram.data(); }
    uint8_t* get_pram() { return pram.data(); }
    uint8_t* get_oam()  { return oam.data();  }

    // Retorna o Framebuffer Interno completo (240x160 pixels de 32-bit ARGB)
    const std::array<uint32_t, SCREEN_WIDTH * SCREEN_HEIGHT>& get_framebuffer() const {
        return framebuffer;
    }

private:
    // Ciclos acumulados na scanline atual
    uint32_t scanline_cycles{0};

    // Registradores de Controle
    DISPCNT  dispcnt{.raw = 0};
    DISPSTAT dispstat{.raw = 0};
    uint16_t vcount{0}; // Line counter (0-227)

    std::array<BGCNT, 4> bgcnt{};
    std::array<uint16_t, 4> bghofs{};
    std::array<uint16_t, 4> bgvofs{};

    // Memórias Dedicadas (VRAM, Palette RAM, OAM)
    alignas(4) std::array<uint8_t, VRAM_SIZE> vram{};
    alignas(4) std::array<uint8_t, PRAM_SIZE> pram{};
    alignas(4) std::array<uint8_t, OAM_SIZE>  oam{};

    // Buffer de Tela Final (ARGB8888)
    std::array<uint32_t, SCREEN_WIDTH * SCREEN_HEIGHT> framebuffer{};

    // Linha de trabalho auxiliar (Linha atual do frame)
    std::array<uint16_t, SCREEN_WIDTH> scanline_buffer{};

    // Métodos Internos do Pipeline de Renderização (Documento - Seção 5)
    void render_scanline();
    void render_mode0();
    void render_mode3();
    void render_mode4();
    void render_mode5();
    void render_sprites();

    // Helpers de Leitura de Cores em Paleta
    Color16 get_bg_palette_color(uint8_t palette_idx, uint8_t color_idx) const;
    Color16 get_obj_palette_color(uint8_t palette_idx, uint8_t color_idx) const;
};

} // namespace zgba::video

#endif // ZGBA_VIDEO_PPU_HPP