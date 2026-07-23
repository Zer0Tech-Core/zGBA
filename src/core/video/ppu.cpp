#include "ppu.hpp"
#include "src/core/memory/bus.hpp"
#include <cstring>
#include <algorithm>

namespace zgba::video {

PPU::PPU() {
    reset();
}

void PPU::reset() {
    scanline_cycles = 0;
    vcount = 0;
    dispcnt.raw = 0;
    dispstat.raw = 0;

    bgcnt.fill(BGCNT{.raw = 0});
    bghofs.fill(0);
    bgvofs.fill(0);

    vram.fill(0);
    pram.fill(0);
    oam.fill(0);
    framebuffer.fill(0xFF000000); // Preto opaco
}

void PPU::step(uint32_t cycles, memory::Bus& bus) {
    scanline_cycles += cycles;

    // Processamento do HBlank (~226 ciclos finais de cada linha)
    if (scanline_cycles >= CYCLES_PER_HDRAW && !dispstat.hblank_flag) {
        dispstat.hblank_flag = 1;
        if (dispstat.hblank_irq_enable) {
            // Trigger IRQ HBlank se configurado (Bit 1 no IF)
            bus.request_interrupt(1 << 1); 
        }
    }

    // Fim da Scanline (~1232 ciclos)
    if (scanline_cycles >= CYCLES_PER_SCANLINE) {
        scanline_cycles -= CYCLES_PER_SCANLINE;
        dispstat.hblank_flag = 0;

        // Renderiza a linha visível se estivermos em VDraw (0..159)
        if (vcount < SCREEN_HEIGHT) {
            render_scanline();
        }

        vcount++;

        // Checagem do VCount Target IRQ
        if (vcount == dispstat.vcount_setting) {
            dispstat.vcount_match_flag = 1;
            if (dispstat.vcount_irq_enable) {
                bus.request_interrupt(1 << 2);
            }
        } else {
            dispstat.vcount_match_flag = 0;
        }

        // Início do VBlank (Linha 160)
        if (vcount == SCREEN_HEIGHT) {
            dispstat.vblank_flag = 1;
            if (dispstat.vblank_irq_enable) {
                bus.request_interrupt(1 << 0); // Trigger IRQ VBlank
            }
        } 
        // Fim da Scanline Vertical Totais (Linha 228 -> Reseta para 0)
        else if (vcount >= TOTAL_SCANLINES) {
            vcount = 0;
            dispstat.vblank_flag = 0;
        }
    }
}

void PPU::render_scanline() {
    // 1. Limpa o buffer da scanline com a cor de fundo padrão (Palette RAM índice 0)
    Color16 backdrop_color = *reinterpret_cast<const Color16*>(&pram[0]);
    std::fill(scanline_buffer.begin(), scanline_buffer.end(), backdrop_color);

    // 2. Se a tela estiver desativada no DISPCNT (bit 7), a tela fica desativada
    if (!dispcnt.forced_blank) {
        int bg_mode = dispcnt.mode;
        switch (bg_mode) {
            case 0: render_mode0(); break;
            case 3: render_mode3(); break;
            case 4: render_mode4(); break;
            case 5: render_mode5(); break;
            default: break;
        }

        if (dispcnt.obj_enable) {
            render_sprites();
        }
    }

    // 3. Copia o scanline_buffer gerado para o framebuffer principal convertendo BGR555 para ARGB8888
    int line_offset = vcount * SCREEN_WIDTH;
    for (int x = 0; x < SCREEN_WIDTH; ++x) {
        framebuffer[line_offset + x] = bgr555_to_argb8888(scanline_buffer[x]);
    }
}

// ------------------------------------------------------------------------------
// Modo 3: Framebuffer Único (240x160, 16 bpp) - (Documento - Seção 3.2)
// ------------------------------------------------------------------------------
void PPU::render_mode3() {
    const uint16_t* vram16 = reinterpret_cast<const uint16_t*>(vram.data());
    uint32_t line_offset = vcount * SCREEN_WIDTH;

    for (int x = 0; x < SCREEN_WIDTH; ++x) {
        scanline_buffer[x] = vram16[line_offset + x];
    }
}

// ------------------------------------------------------------------------------
// Modo 4: Framebuffer Duplo Indexado (240x160, 8 bpp) - (Documento - Seção 3.2)
// ------------------------------------------------------------------------------
void PPU::render_mode4() {
    uint32_t frame_offset = dispcnt.frame_select ? 0xA000 : 0x0000;
    uint32_t line_offset = frame_offset + (vcount * SCREEN_WIDTH);

    for (int x = 0; x < SCREEN_WIDTH; ++x) {
        uint8_t palette_idx = vram[line_offset + x];
        if (palette_idx != 0) { // 0 é transparente
            scanline_buffer[x] = *reinterpret_cast<const uint16_t*>(&pram[palette_idx * 2]);
        }
    }
}

// ------------------------------------------------------------------------------
// Modo 5: Framebuffer Pequeno (160x128, 16 bpp) - (Documento - Seção 3.2)
// ------------------------------------------------------------------------------
void PPU::render_mode5() {
    if (vcount >= 128) return; // Fora do limite do Modo 5

    uint32_t frame_offset = dispcnt.frame_select ? 0xA000 : 0x0000;
    const uint16_t* vram16 = reinterpret_cast<const uint16_t*>(&vram[frame_offset]);
    uint32_t line_offset = vcount * 160;

    for (int x = 0; x < 160; ++x) {
        scanline_buffer[x] = vram16[line_offset + x];
    }
}

// Modo 0 (Tile-based BGs estáticos)
void PPU::render_mode0() {
    // Array para armazenar qual camada desenhou cada pixel (para checagem de prioridade)
    // Prioridades: 0 (mais alta) até 3 (mais baixa). 
    // O backdrop (cor 0) tem prioridade 4.
    std::array<int, SCREEN_WIDTH> pixel_priority;
    pixel_priority.fill(4);

    // Iteramos pelas camadas de fundo da menor prioridade (3) para a maior (0)
    // ou coletamos todas as ativas e ordenamos por prioridade decrescente.
    // No GBA, prioridades menores (0) desenham por cima de prioridades maiores (3).
    
    for (int priority = 3; priority >= 0; --priority) {
        for (int bg_idx = 3; bg_idx >= 0; --bg_idx) {
            // Verifica se o BG está habilitado no DISPCNT e corresponde à prioridade atual
            bool bg_enabled = false;
            if (bg_idx == 0 && dispcnt.bg0_enable) bg_enabled = true;
            if (bg_idx == 1 && dispcnt.bg1_enable) bg_enabled = true;
            if (bg_idx == 2 && dispcnt.bg2_enable) bg_enabled = true;
            if (bg_idx == 3 && dispcnt.bg3_enable) bg_enabled = true;

            if (!bg_enabled) continue;
            if (bgcnt[bg_idx].priority != priority) continue;

            // Configurações do BG
            const auto& cnt = bgcnt[bg_idx];
            uint32_t char_base = (cnt.char_block_base * 16 * 1024);   // Charblock (16KB cada)
            uint32_t screen_base = (cnt.screen_block_base * 2 * 1024); // Screenblock (2KB cada)
            bool is_256_color = cnt.palette_256;                      // 0 = 16/16, 1 = 256/1

            int scroll_x = bghofs[bg_idx];
            int scroll_y = bgvofs[bg_idx];

            // Linha Y no mapa considerando o scroll vertical
            int map_y = (vcount + scroll_y) & 0xFF; // Considerando mapas 256x256 por padrão
            
            for (int x = 0; x < SCREEN_WIDTH; ++x) {
                int map_x = (x + scroll_x) & 0xFF; // Corrigido para o limite correto de 256 pixels

                // Coordenadas do tile no mapa (32x32 tiles por screenblock padrão)
                int tile_col = (map_x / 8) & 31;
                int tile_row = (map_y / 8) & 31;
                
                // Tratamento de tamanhos de mapa maiores (ex: 64x32, 32x64, 64x64)
                // O screen_size (bits 14-15 de BGCNT) define a disposição dos screenblocks.
                uint32_t sb_offset = screen_base;
                // (Lógica de screen_size pode ser expandida aqui para mapas multi-screen)

                uint32_t map_entry_addr = sb_offset + (tile_row * 32 + tile_col) * 2;
                if (map_entry_addr + 1 >= vram.size()) continue;

                uint16_t map_entry = *reinterpret_cast<const uint16_t*>(&vram[map_entry_addr]);
                
                uint16_t tile_num = map_entry & 0x03FF;
                bool h_flip = (map_entry & 0x0400) != 0;
                bool v_flip = (map_entry & 0x0800) != 0;
                uint8_t palette_num = (map_entry >> 12) & 0x000F;

                // Coordenada interna do pixel dentro do tile (0-7)
                int px_in_tile_x = (x + scroll_x) % 8;
                int px_in_tile_y = (vcount + scroll_y) % 8;

                if (h_flip) px_in_tile_x = 7 - px_in_tile_x;
                if (v_flip) px_in_tile_y = 7 - px_in_tile_y;

                // Endereço do tile na VRAM
                uint32_t tile_addr = char_base;
                if (is_256_color) {
                    tile_addr += (tile_num * 64) + (px_in_tile_y * 8) + px_in_tile_x;
                    if (tile_addr >= vram.size()) continue;
                    uint8_t color_idx = vram[tile_addr];
                    if (color_idx != 0) {
                        Color16 color = *reinterpret_cast<const Color16*>(&pram[color_idx * 2]);
                        scanline_buffer[x] = color;
                        pixel_priority[x] = priority;
                    }
                } else {
                    // 16 cores / 16 paletas (4 bpp)
                    tile_addr += (tile_num * 32) + (px_in_tile_y * 4) + (px_in_tile_x / 2);
                    if (tile_addr >= vram.size()) continue;
                    uint8_t byte_val = vram[tile_addr];
                    uint8_t color_idx = (px_in_tile_x % 2 == 0) ? (byte_val & 0x0F) : (byte_val >> 4);
                    
                    if (color_idx != 0) {
                        uint32_t pal_addr = (palette_num * 16 + color_idx) * 2;
                        Color16 color = *reinterpret_cast<const Color16*>(&pram[pal_addr]);
                        scanline_buffer[x] = color;
                        pixel_priority[x] = priority;
                    }
                }
            }
        }
    }
}

// Tabela de tamanhos de sprites do GBA baseada em Shape (attr0 bits 14-15) e Size (attr1 bits 14-15)
const int sprite_dimensions[4][4][2] = {
    // Shape 0: Square
    { {8, 8}, {16, 16}, {32, 32}, {64, 64} },
    // Shape 1: Horizontal
    { {16, 8}, {32, 8}, {32, 16}, {64, 32} },
    // Shape 2: Vertical
    { {8, 16}, {8, 32}, {16, 32}, {32, 64} },
    // Shape 3: Prohibited / Reserved
    { {8, 8}, {8, 8}, {8, 8}, {8, 8} }
};

void PPU::render_sprites() {
    const SpriteOAM* oam_entries = reinterpret_cast<const SpriteOAM*>(oam.data());
    
    // Iteramos de trás para frente (Sprite 127 até 0) para que o sprite 0 tenha prioridade máxima
    for (int i = 127; i >= 0; --i) {
        const auto& sprite = oam_entries[i];

        uint16_t attr0 = sprite.attr0;
        uint16_t attr1 = sprite.attr1;
        uint16_t attr2 = sprite.attr2;

        // Atributo 0
        int obj_y = attr0 & 0x00FF;
        if (obj_y >= 160) obj_y -= 256;

        int obj_mode = (attr0 >> 8) & 0x03;
        if (obj_mode == 2) continue; // Modo semi-transparente/reservado (conforme implementação)

        bool rot_scale = (attr0 & 0x0100) != 0;
        bool attr0_bit9 = (attr0 & 0x0200) != 0;

        // No GBA, o bit 9 do Attr0 tem duplo sentido:
        // - Se rot_scale for falso, bit 9 = 1 significa que o sprite está DESATIVADO.
        // - Se rot_scale for verdadeiro, bit 9 = 1 significa DOUBLE SIZE (dobra o tamanho da bounding box).
        if (!rot_scale && attr0_bit9) continue; 

        // Shape e Tamanho determinam dimensões do sprite usando a tabela
        int shape = (attr0 >> 14) & 0x03;
        int size = (attr1 >> 14) & 0x03;

        int width = sprite_dimensions[shape][size][0];
        int height = sprite_dimensions[shape][size][1];

        if (rot_scale && attr0_bit9) {
            width *= 2;
            height *= 2;
        }

        // Verifica se a linha atual do display (vcount) intersecta o sprite verticalmente
        if (vcount < obj_y || vcount >= obj_y + height) continue;

        int obj_x = attr1 & 0x01FF;
        if (obj_x >= 240) obj_x -= 512; // Wraparound X

        bool h_flip = !rot_scale && ((attr1 & 0x1000) != 0);
        bool v_flip = !rot_scale && ((attr1 & 0x2000) != 0);

        int tile_num = attr2 & 0x03FF;
        //uint8_t priority = (attr2 >> 10) & 0x03;
        uint16_t pal_num = (attr2 >> 12) & 0x000F;
        bool is_256_color = (attr0 & 0x2000) != 0;

        int row_in_obj = vcount - obj_y;
        if (v_flip) row_in_obj = height - 1 - row_in_obj;

        // Renderiza os pixels horizontais do sprite
        for (int x_in_obj = 0; x_in_obj < width; ++x_in_obj) {
            int screen_x = obj_x + x_in_obj;
            if (screen_x < 0 || screen_x >= SCREEN_WIDTH) continue;

            int px_x = h_flip ? (width - 1 - x_in_obj) : x_in_obj;
            int px_y = v_flip ? (height - 1 - row_in_obj) : row_in_obj;

            // Determina qual tile 8x8 interno compõe este pixel no sprite multidimensional
            int tile_width_in_tiles = width / 8;
            int cur_tile_x = px_x / 8;
            int cur_tile_y = px_y / 8;

            int sub_px_x = px_x % 8;
            int sub_px_y = px_y % 8;

            uint32_t target_tile_num = 0;
            // DISPCNT bit 6 define o mapeamento de objetos: 0 = 2D (32 tiles por linha), 1 = 1D (linear)
            if (dispcnt.obj_character_mapping == 1) {
                target_tile_num = tile_num + (cur_tile_y * tile_width_in_tiles) + cur_tile_x;
            } else {
                target_tile_num = tile_num + (cur_tile_y * 32) + cur_tile_x;
            }

            // A VRAM de objetos começa em 0x10000 bytes (64KB) a partir do início da VRAM
            uint32_t obj_vram_base = 0x10000;
            uint32_t tile_bytes = is_256_color ? 64 : 32;
            uint32_t pixel_addr = obj_vram_base + (target_tile_num * tile_bytes);

            uint8_t color_idx = 0;
            if (is_256_color) {
                pixel_addr += (sub_px_y * 8) + sub_px_x;
                if (pixel_addr < vram.size()) {
                    color_idx = vram[pixel_addr];
                }
            } else {
                pixel_addr += (sub_px_y * 4) + (sub_px_x / 2);
                if (pixel_addr < vram.size()) {
                    uint8_t byte_val = vram[pixel_addr];
                    color_idx = (sub_px_x % 2 == 0) ? (byte_val & 0x0F) : (byte_val >> 4);
                }
            }

            // Índice 0 é transparente para sprites
            if (color_idx != 0) {
                // A Palette RAM de Sprites fica na segunda metade da PRAM (offset 0x200 / 512 bytes)
                uint32_t pal_addr = 0x200 + (is_256_color ? (color_idx * 2) : ((pal_num * 16 + color_idx) * 2));
                if (pal_addr + 1 < pram.size()) {
                    Color16 sprite_color = *reinterpret_cast<const Color16*>(&pram[pal_addr]);
                    
                    // Atribui diretamente ao buffer da scanline (comparações de prioridade de camada podem ser refinadas aqui)
                    scanline_buffer[screen_x] = sprite_color;
                }
            }
        }
    }
}

// Leitura / Escrita nos Registradores de Vídeo
uint16_t PPU::read_register(uint32_t addr) const {
    switch (addr & 0x00FFFFFF) {
        case 0x0000: return dispcnt.raw;
        case 0x0004: return dispstat.raw;
        case 0x0006: return vcount;
        case 0x0008: return bgcnt[0].raw;
        case 0x000A: return bgcnt[1].raw;
        case 0x000C: return bgcnt[2].raw;
        case 0x000E: return bgcnt[3].raw;
        default: return 0;
    }
}

void PPU::write_register(uint32_t addr, uint16_t val) {
    switch (addr & 0x00FFFFFF) {
        case 0x0000: dispcnt.raw = val; break;
        case 0x0004: dispstat.raw = (dispstat.raw & 0x07) | (val & ~0x07); break;
        case 0x0008: bgcnt[0].raw = val; break;
        case 0x000A: bgcnt[1].raw = val; break;
        case 0x000C: bgcnt[2].raw = val; break;
        case 0x000E: bgcnt[3].raw = val; break;
        case 0x0010: bghofs[0] = val & 0x01FF; break;
        case 0x0012: bgvofs[0] = val & 0x01FF; break;
        case 0x0014: bghofs[1] = val & 0x01FF; break;
        case 0x0016: bgvofs[1] = val & 0x01FF; break;
        case 0x0018: bghofs[2] = val & 0x01FF; break;
        case 0x001A: bgvofs[2] = val & 0x01FF; break;
        case 0x001C: bghofs[3] = val & 0x01FF; break;
        case 0x001E: bgvofs[3] = val & 0x01FF; break;
        default: break;
    }
}

} // namespace zgba::video