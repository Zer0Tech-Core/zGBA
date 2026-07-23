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
    const uint8_t* get_vram() const { return vram.data(); }
    uint8_t* get_pram() { return pram.data(); }
    const uint8_t* get_pram() const { return pram.data(); }
    uint8_t* get_oam()  { return oam.data(); }
    const uint8_t* get_oam() const { return oam.data(); }

    // Retorna o Framebuffer Interno completo (240x160 pixels de 32-bit ARGB)
    const std::array<uint32_t, SCREEN_WIDTH * SCREEN_HEIGHT>& get_framebuffer() const {
        return framebuffer;
    }

    // Métodos auxiliares para depuração
    bool is_vblank() const { return dispstat.vblank_flag != 0; }
    uint16_t get_vcount() const { return vcount; }

private:
    // Métodos de renderização (apenas um conjunto)
    void render_scanline();
    void render_mode0_scanline();
    void render_mode3_scanline();
    void render_mode4_scanline();
    void render_mode5_scanline();
    void render_sprites_scanline();

    // Métodos auxiliares de renderização
    uint8_t fetch_tile_pixel_4bpp(uint32_t tile_addr, int x, int y) const;
    uint8_t fetch_tile_pixel_8bpp(uint32_t tile_addr, int x, int y) const;
    uint16_t get_palette_color(uint32_t pal_addr) const;
    Color16 get_bg_palette_color(uint8_t palette_idx, uint8_t color_idx) const;
    Color16 get_obj_palette_color(uint8_t palette_idx, uint8_t color_idx) const;

    // Ciclos acumulados na scanline atual
    uint32_t scanline_cycles{0};

    // Registradores de Controle
    DISPCNT  dispcnt;
    DISPSTAT dispstat;
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

    // Buffers de Scanline (tipos corrigidos)
    std::array<uint16_t, SCREEN_WIDTH> scanline_color_buffer{};     // Cores BGR555
    std::array<uint8_t, SCREEN_WIDTH> scanline_priority_buffer{};   // Prioridade (0-4)
    std::array<uint8_t, SCREEN_WIDTH> scanline_source_buffer{};     // 0-3 = BG0-BG3, 4 = OBJ, 5 = Backdrop

    // Linha de trabalho auxiliar (mantido para compatibilidade)
    std::array<uint16_t, SCREEN_WIDTH> scanline_buffer{};
};

} // namespace zgba::video

#endif // ZGBA_VIDEO_PPU_HPP