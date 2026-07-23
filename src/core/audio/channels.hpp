#ifndef ZGBA_AUDIO_CHANNELS_HPP
#define ZGBA_AUDIO_CHANNELS_HPP

#include <cstdint>
#include <array>
#include <queue>

namespace zgba::audio {

// Canal 1: Onda Quadrada com Sweep
struct SquareChannel1 {
    uint16_t cnt_l{0}; // Sweep
    uint16_t cnt_h{0}; // Length, Duty, Envelope
    uint16_t cnt_x{0}; // Frequency, Reset, Loop
    
    bool enabled{false};
    int frequency_timer{0};
    int duty_step{0};
    int current_volume{0};
};

// Canal 2: Onda Quadrada simples
struct SquareChannel2 {
    uint16_t cnt_l{0}; // Length, Duty, Envelope
    uint16_t cnt_h{0}; // Frequency, Reset, Loop
    
    bool enabled{false};
    int frequency_timer{0};
    int duty_step{0};
    int current_volume{0};
};

// Canal 3: Wave RAM
struct WaveChannel {
    uint16_t cnt_l{0}; // Enable, Bank
    uint16_t cnt_h{0}; // Length, Volume
    uint16_t cnt_x{0}; // Frequency, Reset, Loop
    
    std::array<uint8_t, 32> wave_ram{}; // 32 amostras de 4-bit (ou dois bancos de 16)
    bool enabled{false};
    int frequency_timer{0};
    int position{0};
};

// Canal 4: Ruído (LFSR)
struct NoiseChannel {
    uint16_t cnt_l{0}; // Length, Volume, Envelope
    uint16_t cnt_h{0}; // Polynomial counter / Frequency
    
    bool enabled{false};
    int frequency_timer{0};
    uint16_t lfsr{0x7FFF};
};

// Canais Direct Sound (FIFO PCM A e B)
struct DirectSoundChannel {
    std::array<int8_t, 16> fifo{};
    size_t fifo_read_idx{0};
    size_t fifo_write_idx{0};
    size_t fifo_size{0};
    
    uint8_t volume_shift{0}; // 0 = 100%, 1 = 50%, 2 = 25%
    bool enable_right{false};
    bool enable_left{false};
    bool timer_select{false}; // 0 = Timer 0, 1 = Timer 1

    void reset_fifo() {
        fifo.fill(0);
        fifo_read_idx = 0;
        fifo_write_idx = 0;
        fifo_size = 0;
    }

    void push_sample(int8_t sample) {
        if (fifo_size < 16) {
            fifo[fifo_write_idx] = sample;
            fifo_write_idx = (fifo_write_idx + 1) % 16;
            fifo_size++;
        }
    }

    int8_t pop_sample() {
        if (fifo_size > 0) {
            int8_t sample = fifo[fifo_read_idx];
            fifo_read_idx = (fifo_read_idx + 1) % 16;
            fifo_size--;
            return sample;
        }
        return 0;
    }
};

} // namespace zgba::audio

#endif // ZGBA_AUDIO_CHANNELS_HPP