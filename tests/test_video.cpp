#include "src/core/video/ppu.hpp"
#include "src/core/memory/bus.hpp"
#include "src/core/memory/memory_map.hpp"
#include <iostream>
#include <cassert>

void test_ppu_initial_state() {
    zgba::video::PPU ppu;
    assert(ppu.read_register(0x04000000) == 0); // DISPCNT zero
    assert(ppu.read_register(0x04000006) == 0); // VCOUNT zero
    std::cout << "Executando test_ppu_initial_state... [PASSOU]\n";
}

void test_ppu_registers_io() {
    zgba::video::PPU ppu;
    
    // Testa escrita e leitura do DISPCNT (Modo 3, BG0 ativado)
    ppu.write_register(0x04000000, 0x0103);
    uint16_t dispcnt = ppu.read_register(0x04000000);
    assert((dispcnt & 0x7) == 3);
    assert((dispcnt & (1 << 8)) != 0);

    std::cout << "Executando test_ppu_registers_io... [PASSOU]\n";
}

void test_ppu_scanline_timing() {
    zgba::video::PPU ppu;
    zgba::memory::MemoryMap memory_map;
    zgba::memory::Bus bus(memory_map);

    // Executa os ciclos de uma scanline completa (1232 ciclos)
    ppu.step(1232, bus);
    
    // VCount deve avançar para a linha 1
    assert(ppu.read_register(0x04000006) == 1);

    std::cout << "Executando test_ppu_scanline_timing... [PASSOU]\n";
}

void test_ppu_mode3_rendering() {
    zgba::video::PPU ppu;
    
    // Configura o Modo 3 de vídeo
    ppu.write_register(0x04000000, 0x0003);

    // Escreve um pixel vermelho puro (BGR555: R=31) na primeira posição da VRAM
    uint16_t* vram16 = reinterpret_cast<uint16_t*>(ppu.get_vram());
    vram16[0] = 0x001F; // R=31, G=0, B=0

    zgba::memory::MemoryMap memory_map;
    zgba::memory::Bus bus(memory_map);
    // Avança uma scanline para forçar a renderização da linha 0
    ppu.step(1232, bus);

    // Valida se o pixel foi convertido corretamente para ARGB no framebuffer
    const auto& fb = ppu.get_framebuffer();
    uint32_t expected_argb = 0xFFF80000; // Vermelho convertido no canal correto (R=248)
    assert(fb[0] == expected_argb);

    std::cout << "Executando test_ppu_mode3_rendering... [PASSOU]\n";
}

int main() {
    std::cout << "=======================================================\n";
    std::cout << "       zGBA - Suíte de Testes do Subsistema de Vídeo   \n";
    std::cout << "=======================================================\n";

    test_ppu_initial_state();
    test_ppu_registers_io();
    test_ppu_scanline_timing();
    test_ppu_mode3_rendering();

    std::cout << "-------------------------------------------------------\n";
    std::cout << "Resultado Final: 4/4 testes passaram.\n";
    std::cout << "Status: TODOS OS TESTES DE VÍDEO PASSARAM COM SUCESSO!\n";
    return 0;
}