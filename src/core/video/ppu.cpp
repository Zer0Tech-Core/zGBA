// src/core/video/ppu.cpp
#include "ppu.hpp"
#include "src/core/memory/bus.hpp"
#include <cstring>
#include <algorithm>

namespace zgba::video {

// ============================================================================
// CONSTRUTOR E RESET
// ============================================================================

PPU::PPU() {
    reset();
}

void PPU::reset() {
    scanline_cycles = 0;
    vcount = 0;
    dispcnt.raw = 0;
    dispstat.raw = 0;

    for (auto& cnt : bgcnt) cnt.raw = 0;
    bghofs.fill(0);
    bgvofs.fill(0);

    vram.fill(0);
    pram.fill(0);
    oam.fill(0);
    framebuffer.fill(0xFF000000); // Preto opaco

    scanline_color_buffer.fill(0);
    scanline_priority_buffer.fill(4); // Prioridade mais baixa
    scanline_source_buffer.fill(5);   // Backdrop
    scanline_buffer.fill(0);
}

// ============================================================================
// STEP - MÁQUINA DE ESTADOS DA PPU
// ============================================================================

void PPU::step(uint32_t cpu_cycles, memory::Bus& bus) {
    // Acumula os ciclos executados pelo núcleo ARM7TDMI
    scanline_cycles += cpu_cycles;

    // Cada scanline possui exatamente 1232 ciclos no total (1006 de H-Draw + 226 de H-Blank)
    while (scanline_cycles >= CYCLES_PER_SCANLINE) {
        scanline_cycles -= CYCLES_PER_SCANLINE;

        // Se estamos terminando uma linha visível (0 a 159), renderizamos a scanline atual
        if (vcount < SCREEN_HEIGHT) {
            render_scanline();
        }

        // Avança o contador de linhas (VCOUNT) de 0 até 227 (TOTAL_SCANLINES)
        vcount++;
        if (vcount >= TOTAL_SCANLINES) {
            vcount = 0;
        }

        // Atualiza o estado de V-Blank no DISPSTAT (ativo das linhas 160 a 227)
        bool in_vblank = (vcount >= SCREEN_HEIGHT && vcount < TOTAL_SCANLINES);
        dispstat.vblank = in_vblank ? 1 : 0;

        // Verificação de coincidência de V-Counter (V-Match) configurada nos bits 8-15 do DISPSTAT
        uint8_t vmatch_target = (dispstat.raw >> 8) & 0xFF;
        dispstat.vcounter_match = (vcount == vmatch_target) ? 1 : 0;

        // Disparo de interrupção de V-Blank exatamente ao atingir a linha 160
        if (vcount == SCREEN_HEIGHT) {
            if (dispstat.vblank_irq_enable) {
                // TODO: Integrar com o controlador de interrupções (IRQ) quando implementado
                // bus.get_interrupt_controller()->request_interrupt(Interrupt::VBlank);
            }
        }
    }

    // Gerenciamento do H-Blank durante a varredura horizontal da linha visível ativa
    if (vcount < SCREEN_HEIGHT) {
        // Os primeiros 1006 ciclos representam o H-Draw (renderização da linha)
        if (scanline_cycles < CYCLES_PER_HDRAW) {
            dispstat.hblank = 0;
        } else {
            // Os ciclos restantes (226 ciclos) representam o H-Blank
            bool previous_hblank = dispstat.hblank;
            dispstat.hblank = 1;

            // Dispara interrupção de H-Blank na transição exata para o H-Blank, se habilitada
            if (!previous_hblank && dispstat.hblank_irq_enable) {
                // TODO: Integrar com o controlador de interrupções (IRQ) quando implementado
                // bus.get_interrupt_controller()->request_interrupt(Interrupt::HBlank);
            }
        }
    } else {
        // Durante o período de V-Blank, o flag H-Blank também permanece ativo por padrão de hardware
        dispstat.hblank = 1;
    }
}

// ============================================================================
// RENDERIZAÇÃO PRINCIPAL
// ============================================================================

void PPU::render_scanline() {
    // Backdrop padrão (Cor 0 da paleta de BG)
    uint16_t backdrop_color = *reinterpret_cast<uint16_t*>(&pram[0]);
    scanline_color_buffer.fill(backdrop_color);
    scanline_priority_buffer.fill(4); // Prioridade mais baixa
    scanline_source_buffer.fill(5);   // Backdrop

    // Se o display estiver desativado (forced_blank), apenas mostra o backdrop
    if (dispcnt.forced_blank) {
        // Copia o backdrop para o framebuffer
        uint32_t* fb_line = &framebuffer[vcount * SCREEN_WIDTH];
        uint32_t argb_color = bgr555_to_argb8888(backdrop_color);
        for (int x = 0; x < SCREEN_WIDTH; ++x) {
            fb_line[x] = argb_color;
        }
        return;
    }

    // Seleciona o modo de vídeo atual
    switch (dispcnt.video_mode) {
        case 0:
            render_mode0_scanline();
            break;
        case 3:
            render_mode3_scanline();
            break;
        case 4:
            render_mode4_scanline();
            break;
        case 5:
            render_mode5_scanline();
            break;
        default:
            // Modos 1 e 2 (Affine) não implementados ainda
            break;
    }

    // Renderiza Sprites (OBJ) por cima dos fundos conforme prioridades
    if (dispcnt.obj_enable) {
        render_sprites_scanline();
    }

    // Copia a linha processada para o framebuffer final ARGB8888
    uint32_t* fb_line = &framebuffer[vcount * SCREEN_WIDTH];
    for (int x = 0; x < SCREEN_WIDTH; ++x) {
        fb_line[x] = bgr555_to_argb8888(scanline_color_buffer[x]);
    }
}

// ============================================================================
// MODO 0 - TILE-BASED BACKGROUNDS (SCANLINE)
// ============================================================================

void PPU::render_mode0_scanline() {
    // 1. Inicializa o buffer da scanline com a cor do Backdrop (Cor 0 da paleta de BG)
    // O backdrop fica armazenado no início da Palette RAM (pram[0])
    uint16_t backdrop_color = *reinterpret_cast<const uint16_t*>(&pram[0]);
    
    for (int x = 0; x < SCREEN_WIDTH; ++x) {
        scanline_color_buffer[x] = backdrop_color;
        scanline_priority_buffer[x] = 4; // Prioridade mais baixa (atrás de tudo)
        scanline_source_buffer[x] = 5;   // Backdrop
    }

    // 2. Iteração de Camadas (BG3 até BG0)
    // O GBA processa as camadas de fundo de BG3 até BG0. Em caso de empate de prioridade, 
    // camadas com índice menor (como BG0) ganham precedência (por isso iteramos de 3 para 0).
    for (int bg = 3; bg >= 0; --bg) {
        // Verifica no registrador DISPCNT se este fundo específico está ativo (bits 8 a 11)
        bool bg_enabled = (dispcnt.raw & (1 << (8 + bg))) != 0;
        if (!bg_enabled) continue;

        // Extrai as configurações do registrador BGxCNT correspondente
        uint16_t bgcnt_val = bgcnt[bg].raw;
        int char_base_block = (bgcnt_val >> 2) & 0x03;
        int screen_base_block = (bgcnt_val >> 8) & 0x1F;
        bool color_256 = (bgcnt_val >> 7) & 1; // 0 = 4bpp (16 cores), 1 = 8bpp (256 cores)
        int priority = (bgcnt_val & 0x03);      // Prioridade (0 a 3)
        int screenSize = (bgcnt_val >> 14) & 0x03; // Tamanho do mapa de tiles

        // Define as dimensões do mapa de fundo em tiles (padrão GBA)
        int map_width_tiles = 32;
        int map_height_tiles = 32;
        
        switch (screenSize) {
            case 0: map_width_tiles = 32; map_height_tiles = 32; break; // 256 x 256 pixels
            case 1: map_width_tiles = 64; map_height_tiles = 32; break; // 512 x 256 pixels
            case 2: map_width_tiles = 32; map_height_tiles = 64; break; // 256 x 512 pixels
            case 3: map_width_tiles = 64; map_height_tiles = 64; break; // 512 x 512 pixels
        }

        uint32_t char_base_addr = char_base_block * 16384;   // Cada Charblock tem 16 KB
        uint32_t screen_base_addr = screen_base_block * 2048; // Cada Screenblock tem 2 KB

        uint16_t hofs = bghofs[bg] & 0x1FF;
        uint16_t vofs = bgvofs[bg] & 0x1FF;

        // 3. Renderização pixel a pixel ao longo da scanline horizontal (0 a 239)
        for (int x = 0; x < SCREEN_WIDTH; ++x) {
            int px = (x + hofs);
            int py = (vcount + vofs);

            // Tratamento de rolagem e repetição do mapa (Wrap-around)
            int max_width_px = map_width_tiles * 8;
            int max_height_px = map_height_tiles * 8;
            px %= max_width_px;
            py %= max_height_px;

            int tile_x = px / 8;
            int tile_y = py / 8;
            int sub_x = px % 8;
            int sub_y = py % 8;

            // Tratamento para mapas compostos por múltiplos Screenblocks (ex: 64x32)
            int screen_x = tile_x / 32;
            int screen_y = tile_y / 32;
            int local_tile_x = tile_x % 32;
            int local_tile_y = tile_y % 32;

            int screen_id = screen_x + (screen_y * (map_width_tiles / 32));
            uint32_t map_addr = screen_base_addr + (screen_id * 2048) + ((local_tile_y * 32 + local_tile_x) * 2);

            if (map_addr >= VRAM_SIZE) continue;

            // 4. Acesso ao Screenblock na VRAM para pegar os 2 bytes da entrada do tile
            uint16_t tile_entry = *reinterpret_cast<const uint16_t*>(&vram[map_addr]);

            uint16_t tile_idx = tile_entry & 0x03FF;        // Bits 0-9: Índice do tile
            bool h_flip = (tile_entry & 0x0400) != 0;      // Bit 10: Inversão horizontal
            bool v_flip = (tile_entry & 0x0800) != 0;      // Bit 11: Inversão vertical
            uint16_t palette_bank = (tile_entry >> 12) & 0x000F; // Bits 12-15: Banco de paleta (4bpp)

            if (h_flip) sub_x = 7 - sub_x;
            if (v_flip) sub_y = 7 - sub_y;

            uint8_t color_idx = 0;

            // 5. Busca do Pixel no Charblock (Memória de Tiles)
            if (!color_256) {
                // Modo 4bpp (16 cores por paleta, cada tile ocupa 32 bytes)
                uint32_t tile_addr = char_base_addr + (tile_idx * 32) + (sub_y * 4) + (sub_x / 2);
                if (tile_addr >= VRAM_SIZE) continue;

                uint8_t data = vram[tile_addr];
                color_idx = (sub_x & 1) ? (data >> 4) : (data & 0x0F);
            } else {
                // Modo 8bpp (256 cores, paleta única, cada tile ocupa 64 bytes)
                uint32_t tile_addr = char_base_addr + (tile_idx * 64) + (sub_y * 8) + sub_x;
                if (tile_addr >= VRAM_SIZE) continue;

                color_idx = vram[tile_addr];
            }

            // A cor 0 é transparente no fundo; não desenha nada se for 0
            if (color_idx == 0) continue;

            // 6. Consulta da Paleta (Palette RAM / PRAM) para obter a cor real de 15 bits (BGR555)
            uint32_t pram_offset = 0;
            if (!color_256) {
                pram_offset = (palette_bank * 16 + color_idx) * 2;
            } else {
                pram_offset = color_idx * 2;
            }

            if (pram_offset >= PRAM_SIZE) continue;

            uint16_t real_color = *reinterpret_cast<const uint16_t*>(&pram[pram_offset]);

            // 7. Comparação de Prioridade e Escrita no Buffer de Scanline
            // Menor número = maior prioridade (0 é o mais na frente).
            // O operador `<=` garante que fundos menores (como BG0) sobrescrevem os maiores em caso de empate.
            if (priority <= scanline_priority_buffer[x]) {
                scanline_color_buffer[x] = real_color;
                scanline_priority_buffer[x] = priority;
                scanline_source_buffer[x] = bg;
            }
        }
    }
}

// ============================================================================
// MODO 3 - FRAMEBUFFER (240x160, 16bpp)
// ============================================================================

void PPU::render_mode3_scanline() {
    const uint16_t* vram16 = reinterpret_cast<const uint16_t*>(vram.data());
    uint32_t line_offset = vcount * SCREEN_WIDTH;

    for (int x = 0; x < SCREEN_WIDTH; ++x) {
        uint32_t addr = line_offset + x;
        if (addr < VRAM_SIZE / 2) {
            scanline_color_buffer[x] = vram16[addr];
            scanline_priority_buffer[x] = 0; // Prioridade máxima
            scanline_source_buffer[x] = 0;
        }
    }
}

// ============================================================================
// MODO 4 - FRAMEBUFFER INDEXADO (240x160, 8bpp)
// ============================================================================

void PPU::render_mode4_scanline() {
    uint32_t frame_offset = dispcnt.frame_select ? 0xA000 : 0x0000;
    uint32_t line_offset = frame_offset + (vcount * SCREEN_WIDTH);

    for (int x = 0; x < SCREEN_WIDTH; ++x) {
        uint32_t addr = line_offset + x;
        if (addr < VRAM_SIZE) {
            uint8_t palette_idx = vram[addr];
            if (palette_idx != 0) {
                uint16_t color = *reinterpret_cast<const uint16_t*>(&pram[palette_idx * 2]);
                scanline_color_buffer[x] = color;
                scanline_priority_buffer[x] = 0;
                scanline_source_buffer[x] = 0;
            }
        }
    }
}

// ============================================================================
// MODO 5 - FRAMEBUFFER PEQUENO (160x128, 16bpp)
// ============================================================================

void PPU::render_mode5_scanline() {
    if (vcount >= 128) return; // Fora da área renderizável no Modo 5

    uint32_t frame_offset = dispcnt.frame_select ? 0xA000 : 0x0000;
    const uint16_t* vram16 = reinterpret_cast<const uint16_t*>(&vram[frame_offset]);
    uint32_t line_offset = vcount * 160;

    for (int x = 0; x < 160; ++x) {
        uint32_t addr = line_offset + x;
        if (addr < (VRAM_SIZE - frame_offset) / 2) {
            scanline_color_buffer[x] = vram16[addr];
            scanline_priority_buffer[x] = 0;
            scanline_source_buffer[x] = 0;
        }
    }
}

// ============================================================================
// RENDERIZAÇÃO DE SPRITES (SCANLINE)
// ============================================================================

void PPU::render_sprites_scanline() {
    // O GBA possui 128 entradas na OAM.
    // Iteramos de 127 até 0: no GBA, sprites com índice menor (mais próximos de 0) 
    // têm maior prioridade e devem desenhar por cima dos com índice maior. 
    // Processar de trás para frente garante que o índice 0 sobrescreva os demais.
    for (int i = 127; i >= 0; --i) {
        uint32_t oam_addr = i * 8;
        
        uint16_t attr0 = *reinterpret_cast<const uint16_t*>(&oam[oam_addr + 0]);
        uint16_t attr1 = *reinterpret_cast<const uint16_t*>(&oam[oam_addr + 2]);
        uint16_t attr2 = *reinterpret_cast<const uint16_t*>(&oam[oam_addr + 4]);

        // Attr0 Bits 8-9: Object Mode (0 = Normal, 1 = Semi-transparent, 2 = Windowed, 3 = Prohibited/Disabled)
        int obj_mode = (attr0 >> 8) & 0x03;
        if (obj_mode == 3) continue; // Sprite desativado

        bool is_affine = (attr0 & 0x0100) != 0;
        bool double_size = (is_affine && ((attr0 & 0x0200) != 0));

        // Attr0 Bits 14-15 (Shape) e Attr1 Bits 14-15 (Size) definem as dimensões
        int shape = (attr0 >> 14) & 0x03;
        int size = (attr1 >> 14) & 0x03;

        // Tabelas oficiais de tamanhos de sprites do GBA (Width x Height)
        static const int width_table[4][4] = {
            { 8, 16, 32, 64}, // Square (0)
            {16, 32, 32, 64}, // Horizontal (1)
            { 8,  8, 16, 32}, // Vertical (2)
            { 0,  0,  0,  0}  // Prohibited (3)
        };
        static const int height_table[4][4] = {
            { 8, 16, 32, 64}, // Square (0)
            { 8,  8, 16, 32}, // Horizontal (1)
            {16, 32, 32, 64}, // Vertical (2)
            { 0,  0,  0,  0}  // Prohibited (3)
        };

        int width = width_table[shape][size];
        int height = height_table[shape][size];
        if (width == 0 || height == 0) continue;

        int real_height = height;
        if (double_size) real_height *= 2;

        // Posição Y (Attr0 Bits 0-7, tratada com sinal de 8 bits)
        int y_coord = attr0 & 0x00FF;
        if (y_coord >= 160) y_coord -= 256;

        // Verifica se a scanline atual (vcount) intersecta este sprite
        int sprite_y = vcount - y_coord;
        if (sprite_y < 0 || sprite_y >= real_height) continue;

        if (is_affine && double_size) {
            sprite_y /= 2;
        }

        // Posição X (Attr1 Bits 0-8, tratada com sinal de 9 bits)
        int x_coord = attr1 & 0x01FF;
        if (x_coord >= 240) x_coord -= 512;

        // Tratamento de Espelhamento (Flip)
        bool h_flip = (attr1 & 0x1000) != 0;
        bool v_flip = (attr1 & 0x2000) != 0;
        if (v_flip && !is_affine) {
            sprite_y = height - 1 - sprite_y;
        }

        bool color_256 = (attr0 & 0x2000) != 0; // 0 = 4bpp (16 cores), 1 = 8bpp (256 cores)
        int priority = (attr2 >> 10) & 0x03;    // Prioridade (0 a 3)
        uint16_t tile_idx = attr2 & 0x03FF;     // Número do tile inicial
        int palette_bank = (attr2 >> 12) & 0x000F;

        int real_width = width;
        if (is_affine && double_size) real_width *= 2;

        // Renderização pixel a pixel ao longo da largura do sprite na linha atual
        for (int x = 0; x < real_width; ++x) {
            int screen_x = x_coord + x;
            if (screen_x < 0 || screen_x >= SCREEN_WIDTH) continue;

            int sample_x = x;
            int sample_y = sprite_y;

            if (h_flip && !is_affine) {
                sample_x = width - 1 - sample_x;
            }

            // O GBA armazena os tiles de OBJ na segunda metade da VRAM (a partir de 0x10000 bytes)
            bool obj_char_mapping_1d = (dispcnt.raw & 0x0040) != 0;
            uint32_t tile_addr = 0;
            int bytes_per_tile = color_256 ? 64 : 32;

            if (obj_char_mapping_1d) {
                // Mapeamento 1D de VRAM para Objetos
                tile_addr = 0x10000 + (tile_idx * bytes_per_tile);
                int tile_col = sample_x / 8;
                int tile_row = sample_y / 8;
                int sub_x = sample_x % 8;
                int sub_y = sample_y % 8;

                int tiles_per_row = width / 8;
                tile_addr += (tile_row * tiles_per_row + tile_col) * bytes_per_tile;

                uint8_t color_idx = 0;
                if (!color_256) {
                    uint32_t pixel_addr = tile_addr + (sub_y * 4) + (sub_x / 2);
                    if (pixel_addr >= VRAM_SIZE) continue;
                    uint8_t data = vram[pixel_addr];
                    color_idx = (sub_x & 1) ? (data >> 4) : (data & 0x0F);
                } else {
                    uint32_t pixel_addr = tile_addr + (sub_y * 8) + sub_x;
                    if (pixel_addr >= VRAM_SIZE) continue;
                    color_idx = vram[pixel_addr];
                }

                // A cor 0 de sprites é transparente
                if (color_idx == 0) continue;

                // Consulta da Palette RAM de Sprites (A paleta de OBJ começa em 0x200 bytes / 512 bytes na PRAM)
                uint32_t pram_offset = 0x200;
                if (!color_256) {
                    pram_offset += (palette_bank * 16 + color_idx) * 2;
                } else {
                    pram_offset += color_idx * 2;
                }

                if (pram_offset >= PRAM_SIZE) continue;
                uint16_t real_color = *reinterpret_cast<const uint16_t*>(&pram[pram_offset]);

                // Sistema de Prioridade de Pixels:
                // Se a prioridade numérica do sprite for menor ou igual à registrada no buffer do BG, 
                // o sprite fica na frente.
                if (priority <= scanline_priority_buffer[screen_x]) {
                    scanline_color_buffer[screen_x] = real_color;
                    scanline_priority_buffer[screen_x] = priority;
                    scanline_source_buffer[screen_x] = 4; // 4 representa SOURCE_OBJ
                }
            }
        }
    }
}

// ============================================================================
// LEITURA/ESCRITA DE REGISTRADORES
// ============================================================================

uint16_t PPU::read_register(uint32_t addr) const {
    uint32_t offset = addr & 0x00FF;
    switch (offset) {
        case 0x0000: return dispcnt.raw;
        case 0x0004: return dispstat.raw;
        case 0x0006: return vcount;
        case 0x0008: return bgcnt[0].raw;
        case 0x000A: return bgcnt[1].raw;
        case 0x000C: return bgcnt[2].raw;
        case 0x000E: return bgcnt[3].raw;
        case 0x0010: return bghofs[0];
        case 0x0012: return bgvofs[0];
        case 0x0014: return bghofs[1];
        case 0x0016: return bgvofs[1];
        case 0x0018: return bghofs[2];
        case 0x001A: return bgvofs[2];
        case 0x001C: return bghofs[3];
        case 0x001E: return bgvofs[3];
        default:     return 0;
    }
}

void PPU::write_register(uint32_t addr, uint16_t value) {
    uint32_t offset = addr & 0x00FF;
    switch (offset) {
        case 0x0000:
            dispcnt.raw = value;
            break;
        case 0x0004:
            // Bits 0-4 são somente leitura (controlados por hardware), bits 3 a 7 são configuráveis
            dispstat.raw = (dispstat.raw & 0x0007) | (value & 0xFFF8);
            break;
        case 0x0006:
            // VCOUNT é somente leitura, escrita é ignorada pelo hardware real
            break;
        case 0x0008: bgcnt[0].raw = value; break;
        case 0x000A: bgcnt[1].raw = value; break;
        case 0x000C: bgcnt[2].raw = value; break;
        case 0x000E: bgcnt[3].raw = value; break;
        case 0x0010: bghofs[0] = value & 0x1FF; break;
        case 0x0012: bgvofs[0] = value & 0x1FF; break;
        case 0x0014: bghofs[1] = value & 0x1FF; break;
        case 0x0016: bgvofs[1] = value & 0x1FF; break;
        case 0x0018: bghofs[2] = value & 0x1FF; break;
        case 0x001A: bgvofs[2] = value & 0x1FF; break;
        case 0x001C: bghofs[3] = value & 0x1FF; break;
        case 0x001E: bgvofs[3] = value & 0x1FF; break;
        default:
            break;
    }
}

// ============================================================================
// MÉTODOS AUXILIARES
// ============================================================================

uint8_t PPU::fetch_tile_pixel_4bpp(uint32_t tile_addr, int x, int y) const {
    // x e y devem estar no range 0-7
    uint32_t addr = tile_addr + (y * 4) + (x / 2);
    if (addr >= VRAM_SIZE) return 0;
    uint8_t data = vram[addr];
    return (x & 1) ? (data >> 4) : (data & 0x0F);
}

uint8_t PPU::fetch_tile_pixel_8bpp(uint32_t tile_addr, int x, int y) const {
    uint32_t addr = tile_addr + (y * 8) + x;
    if (addr >= VRAM_SIZE) return 0;
    return vram[addr];
}

uint16_t PPU::get_palette_color(uint32_t pal_addr) const {
    if (pal_addr + 1 >= PRAM_SIZE) return 0;
    return *reinterpret_cast<const uint16_t*>(&pram[pal_addr]);
}

Color16 PPU::get_bg_palette_color(uint8_t palette_idx, uint8_t color_idx) const {
    uint32_t pal_addr = (palette_idx * 16 + color_idx) * 2;
    return get_palette_color(pal_addr);
}

Color16 PPU::get_obj_palette_color(uint8_t palette_idx, uint8_t color_idx) const {
    uint32_t pal_addr = 0x200 + (palette_idx * 16 + color_idx) * 2;
    return get_palette_color(pal_addr);
}

} // namespace zgba::video