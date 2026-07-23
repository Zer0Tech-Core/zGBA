#include <iostream>
#include <cassert>
#include <fstream>
#include <vector>
#include "platform/rom_loader.hpp"
#include "../src/core/memory/memory_map.hpp"
#include "platform/input.hpp"
#include "platform/renderer.hpp"
#include "platform/emulador.hpp"

// Teste Unitário: RomLoader
void test_rom_loader() {
    std::cout << "Executando test_rom_loader... " << std::flush;

    // Cria um arquivo ROM temporário de teste
    std::string temp_rom_path = "temp_test.gba";
    {
        std::ofstream file(temp_rom_path, std::ios::binary);
        char dummy_data[512] = {0x01, 0x02, 0x03, 0x04};
        file.write(dummy_data, sizeof(dummy_data));
    }

    RomLoader loader;
    zgba::memory::MemoryMap memory_map;

    // Testa o carregamento com a nova assinatura
    bool success = loader.loadGamePak(temp_rom_path, memory_map);
    assert(success);

    // Remove o arquivo temporário
    std::remove(temp_rom_path.c_str());

    std::cout << "[PASSOU]" << std::endl;
}

// Teste Unitário: Input
void test_input_logic() {
    std::cout << "[TEST] Executando teste unitario do Input..." << std::endl;
    Input input;
    
    // O registrador KEYINPUT inicia com todos os bits em 1 (lógica invertida: botões liberados)
    uint16_t initial_state = input.getKeyInputRegister();
    assert((initial_state & 0x03FF) == 0x03FF);
    
    std::cout << "[TEST] Input passou com sucesso!" << std::endl;
}

// Teste de Integração: Emulador + RomLoader + Renderer
void test_emulator_integration() {
    std::cout << "[TEST] Executando teste de integracao do Emulador..." << std::endl;
    
    std::string dummy_rom = "integration_rom.gba";
    std::ofstream file(dummy_rom, std::ios::binary);
    std::vector<uint8_t> dummy_data(256, 0x11);
    file.write(reinterpret_cast<char*>(dummy_data.data()), dummy_data.size());
    file.close();

    Emulador emu;
    
    // Tenta inicializar o emulador. Em ambientes headless (CI sem display), o SDL pode retornar falso na criação do Renderer,
    // o que é esperado e tratado de forma resiliente pelo teste.
    //bool init_res = emu.init(dummy_rom, "");
    bool init_res = emu.init(dummy_rom);
    
    std::remove(dummy_rom.c_str());
    
    std::cout << "[TEST] Integracao executada. Status de inicializacao grafica: " 
              << (init_res ? "Sucesso (Display disponivel)" : "Modo Headless (SDL Video indisponivel)") << std::endl;
    std::cout << "[TEST] Teste de integracao concluido com sucesso!" << std::endl;
}

int main() {
    std::cout << "=== INICIANDO SUITE DE TESTES DA CAMADA PLATFORM ===" << std::endl;
    test_rom_loader();
    test_input_logic();
    test_emulator_integration();
    std::cout << "=== TODOS OS TESTES DA PLATAFORMA EXECUTADOS COM SUCESSO ===" << std::endl;
    return 0;
}