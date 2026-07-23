#include "src/core/audio/apu.hpp"
#include <cassert>
#include <iostream>
#include <vector>

void test_apu_initialization_and_reset() {
    zgba::audio::APU apu;
    apu.reset();
    
    // Valida o estado inicial do SOUNDBIAS conforme o documento técnico (padrão 0x0200)[cite: 30]
    assert(apu.read_register(0x04000088) == 0x0200);
    std::cout << "[PASS] test_apu_initialization_and_reset\n";
}

void test_apu_registers() {
    zgba::audio::APU apu;
    
    // Testa escrita e leitura em SOUNDCNT_H (0x04000082)[cite: 30]
    apu.write_register(0x04000082, 0x0604);
    assert(apu.read_register(0x04000082) == 0x0604);
    
    // Testa escrita e leitura em SOUNDBIAS (0x04000088)[cite: 30]
    apu.write_register(0x04000088, 0x4200);
    assert(apu.read_register(0x04000088) == 0x4200);
    
    std::cout << "[PASS] test_apu_registers\n";
}

void test_direct_sound_fifo_and_mixing() {
    zgba::audio::APU apu;
    
    // Habilita o circuito mestre de som através de SOUNDCNT_X (Bit 7)[cite: 30]
    apu.write_register(0x04000084, 0x0080);
    
    // Alimenta o FIFO A com amostras de PCM signed de 8 bits via escrita de 32-bit[cite: 30]
    apu.write_fifo_a(0x10203040);
    
    // Coleta amostras de áudio mixadas para o host (estéreo 16-bit)
    std::vector<int16_t> output_buffer;
    apu.get_audio_samples(output_buffer);
    
    // Verifica se o buffer gerou os canais esquerdo e direito corretamente
    assert(output_buffer.size() == 2);
    std::cout << "[PASS] test_direct_sound_fifo_and_mixing\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << " Executando Suíte de Testes da APU (zGBA)\n";
    std::cout << "========================================\n";
    
    test_apu_initialization_and_reset();
    test_apu_registers();
    test_direct_sound_fifo_and_mixing();
    
    std::cout << "========================================\n";
    std::cout << " Todos os testes de áudio passaram!\n";
    std::cout << "========================================\n";
    
    return 0;
}