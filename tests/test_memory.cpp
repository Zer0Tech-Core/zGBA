#include <iostream>
#include <cassert>
#include <cstdint>

// Cabeçalhos do sistema de memória do zGBA
#include "src/core/memory/memory_map.hpp"
#include "src/core/memory/bus.hpp"

// Macros simples de assert para o test runner
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            std::cerr << " [FALHA] " << message << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            return false; \
        } \
    } while(0)

#define RUN_TEST(test_func) \
    do { \
        std::cout << "Executando " << #test_func << "... "; \
        if (test_func()) { \
            std::cout << "\033[32m[PASSOU]\033[0m\n"; \
            passed++; \
        } else { \
            failed++; \
        } \
        total++; \
    } while(0)

using namespace zgba::memory;

// ============================================================================
// TESTES DE UNIDADE: SISTEMA DE MEMÓRIA DO zGBA
// ============================================================================

// 1. Escrita e Leitura na IWRAM (32-bit, Fast RAM)
bool test_iwram_access() {
    MemoryMap mem;
    Bus bus(mem);

    uint32_t addr = ADDR_IWRAM; // 0x03000000

    bus.write32(addr, 0xDEADBEEF);
    TEST_ASSERT(bus.read32(addr) == 0xDEADBEEF, "Escrita/Leitura de 32 bits na IWRAM falhou.");

    bus.write16(addr + 4, 0xCAFE);
    TEST_ASSERT(bus.read16(addr + 4) == 0xCAFE, "Escrita/Leitura de 16 bits na IWRAM falhou.");

    bus.write8(addr + 6, 0xAB);
    TEST_ASSERT(bus.read8(addr + 6) == 0xAB, "Escrita/Leitura de 8 bits na IWRAM falhou.");

    return true;
}

// 2. Acesso à EWRAM e Espelhamento (Mirroring @ 256KB)
bool test_ewram_mirroring() {
    MemoryMap mem;
    Bus bus(mem);

    uint32_t base_addr = ADDR_EWRAM;             // 0x02000000
    uint32_t mirror_addr = ADDR_EWRAM + 0x40000; // 0x02040000 (Offset +256KB)

    bus.write32(base_addr, 0x12345678);
    TEST_ASSERT(bus.read32(mirror_addr) == 0x12345678, "O espelhamento da EWRAM falhou.");

    return true;
}

// 3. Regra de Escrita de 8-bit na VRAM (GBA duplica o byte para 16-bit)
bool test_vram_byte_write() {
    MemoryMap mem;
    Bus bus(mem);

    uint32_t vram_addr = ADDR_VRAM; // 0x06000000

    // Escrever o byte 0xA5 deve resultar em 0xA5A5 ao ler em 16-bit
    bus.write8(vram_addr, 0xA5);
    TEST_ASSERT(bus.read16(vram_addr) == 0xA5A5, "A regra de expansão de byte na VRAM falhou.");

    return true;
}

// 4. Bloqueio de Leitura Direta da BIOS quando PC está fora dela
bool test_bios_read_protection() {
    MemoryMap mem;
    Bus bus(mem);

    // Carrega um valor na BIOS simulada
    mem.bios[0x04] = 0xEA;

    // Simula PC executando a partir do cartucho (0x08000000 >= 0x00004000)
    uint32_t pc_outside_bios = 0x08000000;
    
    // Define um valor prévio no barramento para testar a resposta Open Bus
    bus.last_bus_value = 0xE1A00000;

    uint8_t val = bus.read8(0x00000004, pc_outside_bios);
    
    // Deve retornar o Open Bus e não o byte real da BIOS
    TEST_ASSERT(val != 0xEA, "A BIOS permitiu leitura não autorizada fora da região de inicialização.");

    return true;
}

// 5. Acúmulo de Ciclos de Wait States
bool test_bus_cycles() {
    MemoryMap mem;
    Bus bus(mem);

    // Cast para (void) para satisfazer a regra do [[nodiscard]]
    (void)bus.read32(ADDR_EWRAM);
    TEST_ASSERT(bus.consume_cycles() == 4, "A contagem de ciclos da EWRAM (32-bit) está incorreta.");

    (void)bus.read32(ADDR_IWRAM);
    TEST_ASSERT(bus.consume_cycles() == 1, "A contagem de ciclos da IWRAM (32-bit) está incorreta.");

    return true;
}

// ============================================================================
// MAIN RUNNER
// ============================================================================

int main() {
    std::cout << "\n=======================================================\n";
    std::cout << "      zGBA - Suíte de Testes do Sistema de Memória     \n";
    std::cout << "=======================================================\n\n";

    int total = 0;
    int passed = 0;
    int failed = 0;

    RUN_TEST(test_iwram_access);
    RUN_TEST(test_ewram_mirroring);
    RUN_TEST(test_vram_byte_write);
    RUN_TEST(test_bios_read_protection);
    RUN_TEST(test_bus_cycles);

    std::cout << "\n-------------------------------------------------------\n";
    std::cout << "Resultado Final: " << passed << "/" << total << " testes passaram.\n";

    if (failed > 0) {
        std::cout << "\033[31mStatus: FALHOU (" << failed << " falha(s))\033[0m\n\n";
        return 1;
    }

    std::cout << "\033[32mStatus: TODOS OS TESTES PASSARAM COM SUCESSO!\033[0m\n\n";
    return 0;
}