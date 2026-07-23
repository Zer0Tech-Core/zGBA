#ifndef ZGBA_AUDIO_APU_HPP
#define ZGBA_AUDIO_APU_HPP

#include "channels.hpp"
#include <cstdint>
#include <vector>

namespace zgba::memory {
class Bus;
}

namespace zgba::audio {

class APU {
public:
    APU();
    ~APU() = default;

    void reset();

    // Avança o estado da APU com base nos ciclos da CPU
    void step(uint32_t cpu_cycles);

    // Leitura e Escrita nos Registradores de Áudio (0x04000060 - 0x040000A6)[cite: 29]
    uint16_t read_register(uint32_t addr) const;
    void write_register(uint32_t addr, uint16_t value);

    // Manipulação direta dos FIFOs para Direct Sound (A e B)
    void write_fifo_a(uint32_t val32);
    void write_fifo_b(uint32_t val32);

    // Coleta do buffer de áudio estéreo mixado (16-bit signed) para o host (ex: SDL)
    void get_audio_samples(std::vector<int16_t>& output_buffer);

private:
    SquareChannel1 ch1;
    SquareChannel2 ch2;
    WaveChannel    ch3;
    NoiseChannel   ch4;
    
    DirectSoundChannel dsound_a;
    DirectSoundChannel dsound_b;

    // Registradores de Controle Mestre
    uint16_t soundcnt_l{0};
    uint16_t soundcnt_h{0};
    uint16_t soundcnt_x{0};
    uint16_t soundbias{0x0200}; // Padrão de bias inicial

    std::array<uint8_t, 256> wave_ram_storage{}; // Mapeamento físico da Wave RAM

    // Métodos internos de mixagem por amostra
    int32_t mix_dmg_channels();
    int32_t mix_direct_sound();
};

} // namespace zgba::audio

#endif // ZGBA_AUDIO_APU_HPP