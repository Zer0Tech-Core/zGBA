#ifndef ZGBA_VIDEO_TYPES_HPP
#define ZGBA_VIDEO_TYPES_HPP

#include <cstddef>
#include <cstdint>
#include <array>

namespace zgba::video {

// ============================================================================
// CONSTANTES FÍSICAS DO GBA
// ============================================================================

constexpr int SCREEN_WIDTH     = 240;
constexpr int SCREEN_HEIGHT    = 160;
constexpr int VBLANK_LINES     = 68;
constexpr int TOTAL_SCANLINES  = SCREEN_HEIGHT + VBLANK_LINES; // 228

// Timing de Ciclos por Scanline
constexpr int CYCLES_PER_HDRAW   = 1006;
constexpr int CYCLES_PER_HBLANK  = 226;
constexpr int CYCLES_PER_SCANLINE = CYCLES_PER_HDRAW + CYCLES_PER_HBLANK; // 1232

// Tamanhos de Memória Dedicada de Vídeo
constexpr size_t VRAM_SIZE = 96 * 1024;  // 96 KB
constexpr size_t PRAM_SIZE = 1 * 1024;   // 1 KB
constexpr size_t OAM_SIZE  = 1 * 1024;   // 1 KB

// ============================================================================
// TIPO DE COR BGR555
// ============================================================================

using Color16 = uint16_t;

// Conversão BGR555 -> ARGB8888 com expansão precisa de bits
inline uint32_t bgr555_to_argb8888(uint16_t color16) {
    // Extrai os canais de 5 bits do formato BGR555 do GBA
    uint8_t r5 = (color16 >> 0)  & 0x1F;
    uint8_t g5 = (color16 >> 5)  & 0x1F;
    uint8_t b5 = (color16 >> 10) & 0x1F;

    // Expande linearmente de 5 bits (0-31) para 8 bits (0-255)
    uint8_t r8 = (r5 << 3) | (r5 >> 2);
    uint8_t g8 = (g5 << 3) | (g5 >> 2);
    uint8_t b8 = (b5 << 3) | (b5 >> 2);

    // Retorna no formato ARGB8888 (com Alfa total opaco = 0xFF)
    return 0xFF000000 | (r8 << 16) | (g8 << 8) | b8;
}

// ============================================================================
// REGISTRADOR: DISPCNT (0x04000000) - DISPLAY CONTROL
// ============================================================================

union DISPCNT {
    uint16_t raw;
    struct {
        // Bits 0-2: Modo de vídeo (0-5)
        uint16_t video_mode : 3;
        
        // Bit 3: Modo GBC (read-only, sempre 0 no GBA)
        uint16_t gbc_mode : 1;
        
        // Bit 4: Seleção de frame para display (Modos 4 e 5)
        uint16_t frame_select : 1;
        
        // Bit 5: HBlank Interval Free (permite acesso à VRAM durante HBlank)
        uint16_t hblank_interval_free : 1;
        
        // Bit 6: Mapeamento de caracteres de OBJ (0 = 2D, 1 = 1D)
        uint16_t obj_character_mapping : 1;
        
        // Bit 7: Forced Blank (desativa a tela)
        uint16_t forced_blank : 1;
        
        // Bits 8-11: Habilitação de backgrounds
        uint16_t bg0_enable : 1;
        uint16_t bg1_enable : 1;
        uint16_t bg2_enable : 1;
        uint16_t bg3_enable : 1;
        
        // Bit 12: Habilitação de sprites (OBJ)
        uint16_t obj_enable : 1;
        
        // Bit 13: Habilitação da Window 0
        uint16_t win0_enable : 1;
        
        // Bit 14: Habilitação da Window 1
        uint16_t win1_enable : 1;
        
        // Bit 15: Habilitação da OBJ Window
        uint16_t obj_win_enable : 1;
    };
    
    // Construtores para facilitar o uso
    DISPCNT() : raw(0) {}
    explicit DISPCNT(uint16_t val) : raw(val) {}
    
    // Operadores para conversão implícita
    operator uint16_t() const { return raw; }
    DISPCNT& operator=(uint16_t val) { raw = val; return *this; }
};

// ============================================================================
// REGISTRADOR: DISPSTAT (0x04000004) - DISPLAY STATUS
// ============================================================================

union DISPSTAT {
    uint16_t raw;
    struct {
        // Bit 0: Flag VBlank (0 = VDraw, 1 = VBlank) - READ-ONLY
        uint16_t vblank_flag : 1;
        
        // Bit 1: Flag HBlank (0 = HDraw, 1 = HBlank) - READ-ONLY
        uint16_t hblank_flag : 1;
        
        // Bit 2: Flag VCount Match - READ-ONLY
        uint16_t vcount_match_flag : 1;
        
        // Bit 3: Habilita IRQ VBlank
        uint16_t vblank_irq_enable : 1;
        
        // Bit 4: Habilita IRQ HBlank
        uint16_t hblank_irq_enable : 1;
        
        // Bit 5: Habilita IRQ VCount
        uint16_t vcount_irq_enable : 1;
        
        // Bits 6-7: Reservados (unused)
        uint16_t unused : 2;
        
        // Bits 8-15: Valor de comparação do VCount (0-227)
        uint16_t vcount_setting : 8;
    };
    
    DISPSTAT() : raw(0) {}
    explicit DISPSTAT(uint16_t val) : raw(val) {}
    
    operator uint16_t() const { return raw; }
    DISPSTAT& operator=(uint16_t val) { raw = val; return *this; }
};

// ============================================================================
// REGISTRADOR: BGxCNT (0x04000008 + i*2) - BACKGROUND CONTROL
// ============================================================================

union BGCNT {
    uint16_t raw;
    struct {
        // Bits 0-1: Prioridade (0 = mais alta, 3 = mais baixa)
        uint16_t priority : 2;
        
        // Bits 2-3: Base do Charblock (0-3, cada 16KB)
        uint16_t char_block_base : 2;
        
        // Bits 4-5: Reservados (unused)
        uint16_t unused : 2;
        
        // Bit 6: Mosaico (habilita efeito de mosaico)
        uint16_t mosaic : 1;
        
        // Bit 7: Modo de cor (0 = 4bpp/16 paletas, 1 = 8bpp/256 cores)
        uint16_t color_mode : 1;
        
        // Bits 8-12: Base do Screenblock (0-31, cada 2KB)
        uint16_t screen_block_base : 5;
        
        // Bit 13: Wraparound (apenas para BGs afim - Modos 1 e 2)
        uint16_t wraparound : 1;
        
        // Bits 14-15: Tamanho do mapa
        // 0 = 256x256, 1 = 512x256, 2 = 256x512, 3 = 512x512
        uint16_t screen_size : 2;
    };
    
    BGCNT() : raw(0) {}
    explicit BGCNT(uint16_t val) : raw(val) {}
    
    operator uint16_t() const { return raw; }
    BGCNT& operator=(uint16_t val) { raw = val; return *this; }
    
    // Métodos auxiliares para calcular dimensões do mapa
    int get_map_width_tiles() const {
        return (screen_size & 1) ? 64 : 32;
    }
    
    int get_map_height_tiles() const {
        return (screen_size & 2) ? 64 : 32;
    }
    
    bool is_wide_map() const { return (screen_size & 1) != 0; }
    bool is_tall_map() const { return (screen_size & 2) != 0; }
};

// ============================================================================
// ESTRUTURA DE SPRITE NA OAM
// ============================================================================

struct SpriteOAM {
    uint16_t attr0;  // Atributo 0: Posição Y, Modo, Shape, etc.
    uint16_t attr1;  // Atributo 1: Posição X, Tamanho, Flip, etc.
    uint16_t attr2;  // Atributo 2: Índice do Tile, Prioridade, Paleta, etc.
    uint16_t fill;   // Reservado (para dados de rotação/escala)
};

// ============================================================================
// TABELA DE DIMENSÕES DE SPRITES
// ============================================================================

// Índices: [shape][size] = {width, height}
// shape: 0=Square, 1=Horizontal, 2=Vertical, 3=Reserved
// size: 0=Small, 1=Medium, 2=Large, 3=Extra Large
constexpr int SPRITE_DIMS[4][4][2] = {
    // Shape 0: Square
    {{ 8,  8}, {16, 16}, {32, 32}, {64, 64}},
    // Shape 1: Horizontal
    {{16,  8}, {32,  8}, {32, 16}, {64, 32}},
    // Shape 2: Vertical
    {{ 8, 16}, { 8, 32}, {16, 32}, {32, 64}},
    // Shape 3: Reserved
    {{ 0,  0}, { 0,  0}, { 0,  0}, { 0,  0}}
};

// ============================================================================
// UTILITÁRIOS PARA MANIPULAÇÃO DE BITFIELDS
// ============================================================================

namespace detail {
    // Extrai um campo de bits de um valor
    template<typename T>
    inline T extract_bits(uint16_t value, int shift, int bits) {
        return static_cast<T>((value >> shift) & ((1U << bits) - 1));
    }
    
    // Insere um campo de bits em um valor
    inline uint16_t insert_bits(uint16_t value, uint16_t field, int shift, int bits) {
        uint16_t mask = ((1U << bits) - 1) << shift;
        return (value & ~mask) | ((field << shift) & mask);
    }
}

// ============================================================================
// CONSTANTES ADICIONAIS PARA CONVENIÊNCIA
// ============================================================================

// Constantes para prioridades
constexpr int PRIORITY_HIGHEST = 0;
constexpr int PRIORITY_LOWEST  = 3;
constexpr int PRIORITY_BACKDROP = 4;
constexpr int PRIORITY_OBJ_MIN = 0;
constexpr int PRIORITY_OBJ_MAX = 3;

// Constantes para source buffer
constexpr int SOURCE_BG0     = 0;
constexpr int SOURCE_BG1     = 1;
constexpr int SOURCE_BG2     = 2;
constexpr int SOURCE_BG3     = 3;
constexpr int SOURCE_OBJ     = 4;
constexpr int SOURCE_BACKDROP = 5;

// Constantes para tamanhos de bloco
constexpr size_t CHARBLOCK_SIZE = 16 * 1024;  // 16 KB
constexpr size_t SCREENBLOCK_SIZE = 2 * 1024; // 2 KB

} // namespace zgba::video

#endif // ZGBA_VIDEO_TYPES_HPP