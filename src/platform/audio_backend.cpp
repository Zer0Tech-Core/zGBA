#include "audio_backend.hpp"
#include <SDL2/SDL.h>
#include <algorithm>
#include <iostream>

// Callback executado em thread separada pelo subsistema de áudio
void audio_callback(void* userdata, uint8_t* stream, int len) {
    auto* backend = static_cast<AudioBackend*>(userdata);
    int16_t* output = reinterpret_cast<int16_t*>(stream);
    int num_samples = len / sizeof(int16_t);

    std::lock_guard<std::mutex> lock(backend->audio_mutex);

    for (int i = 0; i < num_samples; ++i) {
        if (backend->read_cursor != backend->write_cursor) {
            output[i] = backend->audio_ring_buffer[backend->read_cursor];
            backend->read_cursor = (backend->read_cursor + 1) % backend->audio_ring_buffer.size();
        } else {
            // Em caso de underrun, injeta silêncio para evitar estalos
            output[i] = 0;
        }
    }
}

AudioBackend::AudioBackend() 
    : device_id(0), is_initialized(false), write_cursor(0), read_cursor(0) {
    audio_ring_buffer.resize(BUFFER_CAPACITY, 0);
}

AudioBackend::~AudioBackend() {
    shutdown();
}

bool AudioBackend::init(uint32_t sample_rate, uint16_t buffer_size) {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        std::cerr << "[AudioBackend] Erro ao inicializar SDL Audio: " << SDL_GetError() << std::endl;
        return false;
    }

    SDL_AudioSpec desired, obtained;
    SDL_zero(desired);
    desired.freq = sample_rate;
    desired.format = AUDIO_S16SYS;
    desired.channels = 2; // Estéreo (GBA nativo)
    desired.samples = buffer_size;
    desired.callback = audio_callback;
    desired.userdata = this;

    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
    if (dev == 0) {
        std::cerr << "[AudioBackend] Erro ao abrir dispositivo de áudio: " << SDL_GetError() << std::endl;
        return false;
    }

    device_id = reinterpret_cast<void*>(static_cast<uintptr_t>(dev));
    is_initialized = true;

    // Inicia a execução do dispositivo de áudio
    SDL_PauseAudioDevice(dev, 0);

    return true;
}

void AudioBackend::shutdown() {
    if (is_initialized && device_id != 0) {
        SDL_AudioDeviceID dev = static_cast<SDL_AudioDeviceID>(reinterpret_cast<uintptr_t>(device_id));
        SDL_CloseAudioDevice(dev);
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        is_initialized = false;
        device_id = 0;
    }
}

void AudioBackend::push_samples(const int16_t* samples, size_t num_samples) {
    if (!is_initialized) return;

    std::lock_guard<std::mutex> lock(audio_mutex);
    for (size_t i = 0; i < num_samples; ++i) {
        size_t next_write = (write_cursor + 1) % audio_ring_buffer.size();
        if (next_write != read_cursor) {
            audio_ring_buffer[write_cursor] = samples[i];
            write_cursor = next_write;
        } else {
            // Buffer cheio, descarta o excedente para evitar travamento de ponteiro
            break;
        }
    }
}

void AudioBackend::pause() {
    if (is_initialized && device_id != 0) {
        SDL_AudioDeviceID dev = static_cast<SDL_AudioDeviceID>(reinterpret_cast<uintptr_t>(device_id));
        SDL_PauseAudioDevice(dev, 1);
    }
}

void AudioBackend::resume() {
    if (is_initialized && device_id != 0) {
        SDL_AudioDeviceID dev = static_cast<SDL_AudioDeviceID>(reinterpret_cast<uintptr_t>(device_id));
        SDL_PauseAudioDevice(dev, 0);
    }
}