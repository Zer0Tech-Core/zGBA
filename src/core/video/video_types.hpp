#ifndef ZGBA_VIDEO_TYPES_HPP
#define ZGBA_VIDEO_TYPES_HPP

#include <cstddef>
#include <cstdint>
#include <array>

namespace zgba::video {

// Constantes Físicas da Tela do GBA (Documento - Seção 1.1)
constexpr int SCREEN_WIDTH  = 240;
constexpr int SCREEN_HEIGHT = 160;
constexpr int VBLANK_LINES  = 68;
constexpr int TOTAL_SCANLINES = SCREEN_HEIGHT + VBLANK_LINES; // 228 scanlines

// Timing de Ciclos por Scanline (~1232 ciclos de clock do sistema por linha)
constexpr int CYCLES_PER_HDRAW = 1006;
constexpr int CYCLES_PER_HBLANK = 226;
constexpr int CYCLES_PER_SCANLINE = CYCLES_PER_HDRAW + CYCLES_PER_HBLANK;

// Tamnhos de Memória Dedicada de Vídeo (Documento - Seção 2)
constexpr size_t VRAM_SIZE    = 96 * 1024; // 96 KB
constexpr size_t PRAM_SIZE    = 1 * 1024;  // 1 KB (512B BG + 512B OBJ)
constexpr size_t OAM_SIZE     = 1 * 1024;  // 1 KB (128 Sprites * 8 Bytes)

// Cor BGR555 em 16-bit (Documento - Seção 4.1)
using Color16 = uint16_t;

// Conversão BGR555 para ARGB8888 (Útil para renderizadores modernos tipo SDL/SFML)
inline uint32_t bgr555_to_argb8888(Color16 color) {
    uint8_t r = (color & 0x001F) << 3;
    uint8_t g = ((color >> 5) & 0x001F) << 3;
    uint8_t b = ((color >> 10) & 0x001F) << 3;
    return 0xFF000000 | (r << 16) | (g << 8) | b;
}

// ------------------------------------------------------------------------------
// Bitfields dos Registradores I/O Principais (Documento - Seção 4)
// ------------------------------------------------------------------------------

// 0x04000000 - DISPCNT (Display Control)
union DISPCNT {
    uint16_t raw;
    struct {
        uint16_t mode : 3;              // Modo de Vídeo (0-5)
        uint16_t gbc_mode : 1;          // Bit GBC (Read Only)
        uint16_t frame_select : 1;      // Page Flipping (Modos 4/5)
        uint16_t hblank_interval_free : 1;
        uint16_t obj_character_mapping : 1; // 0 = 2D, 1 = 1D
        uint16_t forced_blank : 1;      // Display Off
        uint16_t bg0_enable : 1;
        uint16_t bg1_enable : 1;
        uint16_t bg2_enable : 1;
        uint16_t bg3_enable : 1;
        uint16_t obj_enable : 1;        // Ativação de Sprites
        uint16_t win0_enable : 1;
        uint16_t win1_enable : 1;
        uint16_t obj_win_enable : 1;
    };
};

// 0x04000004 - DISPSTAT (Display Status)
union DISPSTAT {
    uint16_t raw;
    struct {
        uint16_t vblank_flag : 1;       // Status VBlank (0 = VDraw, 1 = VBlank)
        uint16_t hblank_flag : 1;       // Status HBlank
        uint16_t vcount_match_flag : 1; // VCount Match Status
        uint16_t vblank_irq_enable : 1; // Habilita IRQ VBlank
        uint16_t hblank_irq_enable : 1; // Habilita IRQ HBlank
        uint16_t vcount_irq_enable : 1; // Habilita IRQ VCount
        uint16_t unused : 2;
        uint16_t vcount_setting : 8;    // Target Scanline IRQ (0-227)
    };
};

// 0x04000008 + i*2 - BGxCNT (Background Control)
union BGCNT {
    uint16_t raw;
    struct {
        uint16_t priority : 2;          // Prioridade (0 = mais alta)
        uint16_t char_block_base : 2;   // Base dos Tiles em VRAM (Blocos 16KB)
        uint16_t unused : 2;
        uint16_t mosaic : 1;
        uint16_t palette_256 : 1;       // 0 = 16/16, 1 = 256/1
        uint16_t screen_block_base : 5; // Base do Mapa em VRAM (Blocos 2KB)
        uint16_t wraparound : 1;        // Apenas BGs Afim
        uint16_t screen_size : 2;       // Tamanho do mapa
    };
};

// Atributos de Sprites na OAM (Documento - Seção 2.2)
struct SpriteOAM {
    uint16_t attr0;
    uint16_t attr1;
    uint16_t attr2;
    uint16_t fill; // Reservado/Dados Afim extras
};

} // namespace zgba::video

#endif // ZGBA_VIDEO_TYPES_HPP