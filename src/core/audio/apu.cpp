#include "apu.hpp"
#include <algorithm>
#include <cstring>

namespace zgba::audio {

APU::APU() {
    reset();
}

void APU::reset() {
    ch1 = {};
    ch2 = {};
    ch3 = {};
    ch4 = {};
    dsound_a.reset_fifo();
    dsound_b.reset_fifo();

    soundcnt_l = 0;
    soundcnt_h = 0;
    soundcnt_x = 0;
    soundbias  = 0x0200;
    wave_ram_storage.fill(0);
}

void APU::step(uint32_t cpu_cycles) {
    (void)cpu_cycles; // Silencia o aviso de parâmetro não utilizado
    
    // TODO: Decrementar temporizadores internos de frequência, envelopes e sweep dos canais DMG
    // O Direct Sound consome amostras acionado pelos Timers (Timer 0 / Timer 1)
}

uint16_t APU::read_register(uint32_t addr) const {
    uint32_t offset = addr & 0x00FF;
    switch (offset) {
        case 0x0080: return soundcnt_l;
        case 0x0082: return soundcnt_h;
        case 0x0084: return soundcnt_x;
        case 0x0088: return soundbias;
        default:
            // Leitura de Wave RAM ou registradores específicos de canais
            if (offset >= 0x0090 && offset <= 0x009E) {
                // Retorna dados da Wave RAM
                uint16_t val;
                std::memcpy(&val, &wave_ram_storage[offset - 0x0090], sizeof(uint16_t));
                return val;
            }
            return 0;
    }
}

void APU::write_register(uint32_t addr, uint16_t value) {
    uint32_t offset = addr & 0x00FF;
    switch (offset) {
        case 0x0060: ch1.cnt_l = value; break;
        case 0x0062: ch1.cnt_h = value; break;
        case 0x0064: ch1.cnt_x = value; break;
        
        case 0x0068: ch2.cnt_l = value; break;
        case 0x006C: ch2.cnt_h = value; break;

        case 0x0070: ch3.cnt_l = value; break;
        case 0x0072: ch3.cnt_h = value; break;
        case 0x0074: ch3.cnt_x = value; break;

        case 0x0078: ch4.cnt_l = value; break;
        case 0x007C: ch4.cnt_h = value; break;

        case 0x0080: soundcnt_l = value; break;
        case 0x0082: 
            soundcnt_h = value;
            dsound_a.volume_shift = (value & (1 << 2)) ? 1 : 0; // Bit 2 define volume 50% ou 100%
            dsound_b.volume_shift = (value & (1 << 3)) ? 1 : 0;
            if (value & (1 << 11)) dsound_a.reset_fifo(); // Reset FIFO A
            if (value & (1 << 19)) dsound_b.reset_fifo(); // Reset FIFO B
            break;
        case 0x0084: 
            soundcnt_x = value;
            if (!(value & 0x80)) {
                reset(); // Mestre desativado (Master Sound Enable bit 7)
            }
            break;
        case 0x0088: 
            // Bits 0-9 bias (não alterar), bits 14-15 PWM resolution[cite: 29]
            soundbias = value; 
            break;
        default:
            if (offset >= 0x0090 && offset <= 0x009E) {
                std::memcpy(&wave_ram_storage[offset - 0x0090], &value, sizeof(uint16_t));
            }
            break;
    }
}

void APU::write_fifo_a(uint32_t val32) {
    // O FIFO aceita escrita de 32 bits (4 amostras de 8 bits)[cite: 29]
    dsound_a.push_sample(static_cast<int8_t>(val32 & 0xFF));
    dsound_a.push_sample(static_cast<int8_t>((val32 >> 8) & 0xFF));
    dsound_a.push_sample(static_cast<int8_t>((val32 >> 16) & 0xFF));
    dsound_a.push_sample(static_cast<int8_t>((val32 >> 24) & 0xFF));
}

void APU::write_fifo_b(uint32_t val32) {
    dsound_b.push_sample(static_cast<int8_t>(val32 & 0xFF));
    dsound_b.push_sample(static_cast<int8_t>((val32 >> 8) & 0xFF));
    dsound_b.push_sample(static_cast<int8_t>((val32 >> 16) & 0xFF));
    dsound_b.push_sample(static_cast<int8_t>((val32 >> 24) & 0xFF));
}

/*int32_t APU::mix_dmg_channels() {
    // Implementar geração de onda dos canais 1, 2, 3 e 4
    return 0;
}

int32_t APU::mix_direct_sound() {
    int32_t sample_a = dsound_a.fifo_size > 0 ? dsound_a.pop_sample() : 0;
    int32_t sample_b = dsound_b.fifo_size > 0 ? dsound_b.pop_sample() : 0;
    
    // Aplicação simples de escala de volume
    sample_a >>= dsound_a.volume_shift;
    sample_b >>= dsound_b.volume_shift;

    return sample_a + sample_b;
}*/

void APU::get_audio_samples(std::vector<int16_t>& output_buffer) {
    // Mixagem final combinando DMG e Direct Sound em estéreo (Left/Right)
    int32_t mixed = mix_dmg_channels() + mix_direct_sound();
    
    // Expande para 16-bit signed para evitar clipping na saída do host
    int16_t final_sample = static_cast<int16_t>(std::clamp(mixed * 256, -32768, 32767));
    
    output_buffer.push_back(final_sample); // Canal Esquerdo
    output_buffer.push_back(final_sample); // Canal Direito
}

} // namespace zgba::audio