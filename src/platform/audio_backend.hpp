#ifndef AUDIO_BACKEND_HPP
#define AUDIO_BACKEND_HPP

#include <cstdint>
#include <vector>
#include <mutex>

class AudioBackend {
public:
    AudioBackend();
    ~AudioBackend();

    bool init(uint32_t sample_rate, uint16_t buffer_size);
    void shutdown();

    // Enfileira amostras de áudio estéreo (L/R) geradas pela APU
    void push_samples(const int16_t* samples, size_t num_samples);

    // Controle de reprodução
    void pause();
    void resume();

    [[nodiscard]] bool is_ready() const { return is_initialized; }

private:
    void* device_id; // Armazena SDL_AudioDeviceID de forma opaca
    bool is_initialized;

    // Buffer circular thread-safe para sincronização entre o core do GBA e a thread de áudio
    static constexpr size_t BUFFER_CAPACITY = 4096 * 8;
    std::vector<int16_t> audio_ring_buffer;
    size_t write_cursor;
    size_t read_cursor;
    std::mutex audio_mutex;

    // Callback amigável para C exigido pelo SDL
    friend void audio_callback(void* userdata, uint8_t* stream, int len);
};

#endif // AUDIO_BACKEND_HPP