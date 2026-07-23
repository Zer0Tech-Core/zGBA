#include <iostream>
#include <string>
#include "emulador.hpp"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Uso: zGBA <caminho_para_rom.gba> [--debug]" << std::endl;
        return 1;
    }

    std::string rom_path = "";
    bool debug_mode = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--debug" || arg == "-d") {
            debug_mode = true;
        } else if (rom_path.empty()) {
            rom_path = arg;
        }
    }

    if (rom_path.empty()) {
        std::cerr << "Erro: Nenhum arquivo de ROM especificado." << std::endl;
        return 1;
    }

    Emulador zGBA;

    // Inicializa passando apenas a ROM (a BIOS já está embutida no binário)
    if (!zGBA.init(rom_path)) {
        std::cerr << "Falha ao inicializar o emulador zGBA." << std::endl;
        return 1;
    }

    if (debug_mode) {
        zGBA.enable_debugger(true, 0x08000000);
        std::cout << "[DEBUGGER] Ativado via linha de comando. Breakpoint em 0x08000000." << std::endl;
    }

    std::cout << "Emulador iniciado com sucesso. Executando..." << std::endl;
    zGBA.run();

    return 0;
}