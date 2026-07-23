#include "apu.hpp"
#include <algorithm>
#include <cmath>

namespace zgba::audio {

// Tabelas de Duty Cycle para as ondas quadradas (Canais 1 e 2)[cite: 29]
static constexpr uint8_t DUTY_TABLE[4][8] = {
    {0, 0, 0, 0, 0, 0, 0, 1}, // 12.5%
    {0, 0, 0, 0, 0, 0, 1, 1}, // 25.0%
    {0, 0, 0, 0, 1, 1, 1, 1}, // 50.0%
    {1, 1, 1, 1, 1, 1, 0, 0}  // 75.0%
};

int32_t APU::mix_dmg_channels() {
    int32_t output = 0;

    // Canal 1: Onda Quadrada com Sweep[cite: 29]
    if (ch1.enabled) {
        uint8_t duty = (ch1.cnt_h >> 6) & 0x3;
        int wave_val = DUTY_TABLE[duty][ch1.duty_step] ? 1 : -1;
        output += wave_val * 8; 
    }

    // Canal 2: Onda Quadrada simples[cite: 29]
    if (ch2.enabled) {
        uint8_t duty = (ch2.cnt_l >> 6) & 0x3;
        int wave_val = DUTY_TABLE[duty][ch2.duty_step] ? 1 : -1;
        output += wave_val * 8;
    }

    // Canal 3: Wave RAM (64 amostras de 4 bits / 32 bytes)[cite: 29]
    if (ch3.enabled) {
        uint8_t sample_4bit = wave_ram_storage[ch3.position / 2];
        if ((ch3.position % 2) == 0) {
            sample_4bit >>= 4; // Nibble superior
        } else {
            sample_4bit &= 0x0F; // Nibble inferior
        }
        // Converte valor de 4 bits (0 a 15) para signed (-8 a 7)
        int wave_val = static_cast<int>(sample_4bit) - 8;
        output += wave_val * 4;
    }

    // Canal 4: Ruído pseudo-aleatório (LFSR)[cite: 29]
    if (ch4.enabled) {
        int noise_val = (ch4.lfsr & 1) ? -8 : 8;
        output += noise_val;
    }

    return output;
}

int32_t APU::mix_direct_sound() {
    // Canais Direct Sound A e B processam amostras PCM de 8 bits (signed) via FIFO[cite: 29]
    int32_t sample_a = dsound_a.fifo_size > 0 ? dsound_a.pop_sample() : 0;
    int32_t sample_b = dsound_b.fifo_size > 0 ? dsound_b.pop_sample() : 0;
    
    // Aplicação do atenuador de volume configurado em SOUNDCNT_H (0 = 100%, 1 = 50%)[cite: 29]
    sample_a >>= dsound_a.volume_shift;
    sample_b >>= dsound_b.volume_shift;

    return (sample_a + sample_b) * 4;
}

} // namespace zgba::audio