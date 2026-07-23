#include <iostream>
#include <cassert>
#include <vector>
#include <iomanip>
#include <unordered_map>

#include "src/core/cpu/arm7tdmi/registers.hpp"
#include "src/core/cpu/arm7tdmi/exceptions.hpp"
#include "src/core/cpu/arm7tdmi/pipeline.hpp"
#include "src/core/cpu/thumb/thumb_decoder.hpp"
#include "src/core/cpu/cpu.hpp"

using namespace zGBA::CPU::ARM7TDMI;
using namespace zGBA::CPU::ARM7TDMI::Thumb;

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            std::cout << "\033[31m[FALHA]\033[0m " << message << " (Linha " << __LINE__ << ")\n"; \
            return false; \
        } \
    } while (0)

#define RUN_TEST(test_func) \
    do { \
        std::cout << "[EXECUTANDO] " << #test_func << "... "; \
        total_tests++; \
        if (test_func()) { \
            std::cout << "\033[32m[OK]\033[0m\n"; \
            passed_tests++; \
        } else { \
            failed_tests++; \
        } \
    } while (0)

// Memória simulada para os testes de Load/Store e Pipeline
struct MockMemory {
    std::unordered_map<uint32_t, uint8_t> bytes;

    uint8_t read8(uint32_t addr) const {
        auto it = bytes.find(addr);
        return (it != bytes.end()) ? it->second : 0;
    }

    uint16_t read16(uint32_t addr) const {
        return read8(addr) | (static_cast<uint16_t>(read8(addr + 1)) << 8);
    }

    uint32_t read32(uint32_t addr) const {
        return read8(addr) | 
              (static_cast<uint32_t>(read8(addr + 1)) << 8) |
              (static_cast<uint32_t>(read8(addr + 2)) << 16) |
              (static_cast<uint32_t>(read8(addr + 3)) << 24);
    }

    void write8(uint32_t addr, uint8_t val) { bytes[addr] = val; }
    void write16(uint32_t addr, uint16_t val) {
        bytes[addr] = val & 0xFF;
        bytes[addr + 1] = (val >> 8) & 0xFF;
    }
    void write32(uint32_t addr, uint32_t val) {
        bytes[addr] = val & 0xFF;
        bytes[addr + 1] = (val >> 8) & 0xFF;
        bytes[addr + 2] = (val >> 16) & 0xFF;
        bytes[addr + 3] = (val >> 24) & 0xFF;
    }
};

// ==========================================
// 1. Testes de Registradores e Banking
// ==========================================

bool test_registers_reset_state() {
    Registers regs;
    regs.reset();
    TEST_ASSERT(regs.get_pc() == 0x00000000, "PC inicial deve ser 0x00000000");
    TEST_ASSERT(regs.get_mode() == Mode::Supervisor, "Modo inicial deve ser Supervisor (0x13)");
    TEST_ASSERT(regs.is_flag_set(Registers::Flag::I), "IRQ deve estar desabilitado no reset");
    TEST_ASSERT(regs.is_flag_set(Registers::Flag::F), "FIQ deve estar desabilitado no reset");
    TEST_ASSERT(!regs.is_thumb(), "CPU deve iniciar no modo ARM");
    return true;
}

bool test_registers_banking_fiq() {
    Registers regs;
    regs.reset();
    regs.set_mode(Mode::System);
    for (uint32_t i = 8; i <= 14; ++i) regs.write(i, 0x11111111U * i);

    regs.set_mode(Mode::FIQ);
    for (uint32_t i = 8; i <= 14; ++i) {
        TEST_ASSERT(regs.read(i) == 0, "Registradores FIQ R8-R14 devem iniciar zerados");
        regs.write(i, 0x99999999);
    }

    regs.set_mode(Mode::System);
    for (uint32_t i = 8; i <= 14; ++i) {
        TEST_ASSERT(regs.read(i) == 0x11111111U * i, "R8-R14 do modo System foram corrompidos pelo FIQ");
    }
    return true;
}

bool test_registers_sp_lr_banking_modes() {
    Registers regs;
    regs.reset();
    regs.set_mode(Mode::IRQ);
    regs.write(13, 0x03007FE0);
    regs.write(14, 0x08000100);

    regs.set_mode(Mode::Supervisor);
    regs.write(13, 0x03007FFF);
    regs.write(14, 0x08000200);

    regs.set_mode(Mode::IRQ);
    TEST_ASSERT(regs.read(13) == 0x03007FE0, "SP_irq corrompido");
    TEST_ASSERT(regs.read(14) == 0x08000100, "LR_irq corrompido");
    return true;
}

// ==========================================
// 2. Testes de Tratamento de Exceções
// ==========================================

bool test_exception_swi_handling() {
    Registers regs;
    regs.reset();
    regs.set_mode(Mode::User);
    regs.set_flag(Registers::Flag::T, false);

    const uint32_t current_pc = 0x08000008;
    regs.set_pc(current_pc);

    Exceptions::raise(regs, ExceptionType::SoftwareInterrupt);

    TEST_ASSERT(regs.get_mode() == Mode::Supervisor, "SWI deve alternar para modo Supervisor");
    TEST_ASSERT(regs.get_pc() == 0x00000008, "PC deve ir para o vetor SWI (0x08)");
    TEST_ASSERT(regs.read(14) == (current_pc - 4), "LR_svc deve salvar a instrução seguinte");
    TEST_ASSERT(regs.is_flag_set(Registers::Flag::I), "SWI deve desabilitar IRQ");
    TEST_ASSERT(!regs.is_thumb(), "Exceção deve forçar estado ARM");
    return true;
}

// ==========================================
// 3. Testes do Decoder Thumb
// ==========================================

bool test_thumb_alu_operations() {
    Registers regs;
    regs.reset();
    regs.set_flag(Registers::Flag::T, true);

    MockMemory mem;
    auto r8  = [&](uint32_t a) { return mem.read8(a); };
    auto r16 = [&](uint32_t a) { return mem.read16(a); };
    auto r32 = [&](uint32_t a) { return mem.read32(a); };
    auto w8  = [&](uint32_t a, uint8_t v) { mem.write8(a, v); };
    auto w16 = [&](uint32_t a, uint16_t v) { mem.write16(a, v); };
    auto w32 = [&](uint32_t a, uint32_t v) { mem.write32(a, v); };

    // 1. MOV R0, #42 (Opcode: 0x202A)
    ThumbDecoder::decode_and_execute(0x202A, regs, r8, r16, r32, w8, w16, w32);
    TEST_ASSERT(regs.read(0) == 42, "MOV R0, #42 falhou no modo Thumb");

    // 2. ADD R1, R0, #5 (Opcode: 0x1D41)
    ThumbDecoder::decode_and_execute(0x1D41, regs, r8, r16, r32, w8, w16, w32);
    TEST_ASSERT(regs.read(1) == 47, "ADD R1, R0, #5 falhou no modo Thumb");

    // 3. LSL R2, R1, #2 (Opcode: 0x008A) -> R2 = 47 << 2 = 188
    ThumbDecoder::decode_and_execute(0x008A, regs, r8, r16, r32, w8, w16, w32);
    TEST_ASSERT(regs.read(2) == 188, "LSL R2, R1, #2 falhou no modo Thumb");

    return true;
}

bool test_thumb_branch_bx() {
    Registers regs;
    regs.reset();
    regs.set_flag(Registers::Flag::T, true);

    MockMemory mem;
    auto r8  = [&](uint32_t a) { return mem.read8(a); };
    auto r16 = [&](uint32_t a) { return mem.read16(a); };
    auto r32 = [&](uint32_t a) { return mem.read32(a); };
    auto w8  = [&](uint32_t a, uint8_t v) { mem.write8(a, v); };
    auto w16 = [&](uint32_t a, uint16_t v) { mem.write16(a, v); };
    auto w32 = [&](uint32_t a, uint32_t v) { mem.write32(a, v); };

    regs.write(0, 0x08000100);

    bool flushed = ThumbDecoder::decode_and_execute(0x4700, regs, r8, r16, r32, w8, w16, w32);
    TEST_ASSERT(flushed, "BX deve indicar necessidade de flush no pipeline");
    TEST_ASSERT(!regs.is_thumb(), "BX para endereço com bit 0=0 deve alternar para modo ARM");
    TEST_ASSERT(regs.get_pc() == 0x08000100, "PC incorreto após BX");

    return true;
}

bool test_thumb_load_store_push_pop() {
    Registers regs;
    regs.reset();
    regs.set_flag(Registers::Flag::T, true);
    regs.write(13, 0x03007FFF);

    MockMemory mem;
    auto r8  = [&](uint32_t a) { return mem.read8(a); };
    auto r16 = [&](uint32_t a) { return mem.read16(a); };
    auto r32 = [&](uint32_t a) { return mem.read32(a); };
    auto w8  = [&](uint32_t a, uint8_t v) { mem.write8(a, v); };
    auto w16 = [&](uint32_t a, uint16_t v) { mem.write16(a, v); };
    auto w32 = [&](uint32_t a, uint32_t v) { mem.write32(a, v); };

    regs.write(0, 0xDEADBEEF);
    regs.write(1, 0xCAFEBABE);
    regs.write(14, 0x08000041);

    ThumbDecoder::decode_and_execute(0xB503, regs, r8, r16, r32, w8, w16, w32);
    TEST_ASSERT(regs.read(13) == (0x03007FFF - 12), "SP decrementado incorretamente no PUSH");

    regs.write(0, 0);
    regs.write(1, 0);

    ThumbDecoder::decode_and_execute(0xBD03, regs, r8, r16, r32, w8, w16, w32);
    TEST_ASSERT(regs.read(0) == 0xDEADBEEF, "R0 restaurado incorretamente pelo POP");
    TEST_ASSERT(regs.read(1) == 0xCAFEBABE, "R1 restaurado incorretamente pelo POP");
    TEST_ASSERT(regs.get_pc() == 0x08000040, "PC carregado com bit 0 removido do endereço de retorno");
    TEST_ASSERT(regs.read(13) == 0x03007FFF, "SP restaurado incorretamente pelo POP");

    return true;
}

// ==========================================
// 4. Testes do Pipeline ARM
// ==========================================

bool test_pipeline_arm_execution() {
    Registers regs;
    Pipeline pipeline;
    regs.reset();

    auto mock_read32 = [](uint32_t addr) -> uint32_t {
        switch (addr) {
            case 0x08000000: return 0xE1A00000; // NOP
            case 0x08000004: return 0xE2800001; // ADD R0, R0, #1
            case 0x08000008: return 0xE2411001; // SUB R1, R1, #1
            default: return 0x00000000;
        }
    };
    auto mock_read16 = [](uint32_t) -> uint16_t { return 0x0000; };

    regs.set_pc(0x08000000);
    pipeline.flush(regs, mock_read32, mock_read16);

    TEST_ASSERT(regs.get_pc() == 0x08000008, "PC adiantado +8 após o flush inicial");

    auto stage1 = pipeline.step(regs, mock_read32, mock_read16);
    TEST_ASSERT(stage1.valid && stage1.address == 0x08000000, "Estágio de execução incorreto");
    TEST_ASSERT(stage1.instruction == 0xE1A00000, "Opcode incorreto");

    return true;
}

bool test_cpu_class_integration() {
    MockMemory mem;
    CPU cpu;

    // Configura o Barramento na CPU
    cpu.set_bus({
        [&](uint32_t a) { return mem.read8(a); },
        [&](uint32_t a) { return mem.read16(a); },
        [&](uint32_t a) { return mem.read32(a); },
        [&](uint32_t a, uint8_t v) { mem.write8(a, v); },
        [&](uint32_t a, uint16_t v) { mem.write16(a, v); },
        [&](uint32_t a, uint32_t v) { mem.write32(a, v); }
    });

    // Reseta a CPU (Seta PC=0x0, Modo Supervisor, ARM)
    cpu.reset();
    TEST_ASSERT(cpu.get_registers().get_pc() == 0x00000008, "PC inicial do pipeline ARM deve ser 0x08");

    // Simula disparo de SWI
    cpu.raise_exception(ExceptionType::SoftwareInterrupt);
    TEST_ASSERT(cpu.get_registers().get_mode() == Mode::Supervisor, "Deve estar em Supervisor após SWI");
    TEST_ASSERT(cpu.get_registers().get_pc() == 0x00000010, "PC no vetor de SWI (0x08 + 8 pipeline)");

    return true;
}

// ==========================================
// Ponto de Entrada Principal (Runner)
// ==========================================

int main() {
    std::cout << "====================================================\n";
    std::cout << "         zGBA - CPU Core Unit Test Suite            \n";
    std::cout << "====================================================\n\n";

    int total_tests = 0, passed_tests = 0, failed_tests = 0;

    // Registradores & Exceções
    RUN_TEST(test_registers_reset_state);
    RUN_TEST(test_registers_banking_fiq);
    RUN_TEST(test_registers_sp_lr_banking_modes);
    RUN_TEST(test_exception_swi_handling);

    // Pipeline ARM
    RUN_TEST(test_pipeline_arm_execution);

    // Decoders Thumb
    RUN_TEST(test_thumb_alu_operations);
    RUN_TEST(test_thumb_branch_bx);
    RUN_TEST(test_thumb_load_store_push_pop);

    std::cout << "\n====================================================\n";
    std::cout << "RESULTADO: " << passed_tests << "/" << total_tests << " testes passaram.\n";
    std::cout << "====================================================\n";

    return (failed_tests == 0) ? 0 : 1;
}