#pragma once
#include <cstdint>
#include <SDL.h>

class Input {
public:
    Input();
    ~Input() = default;

    void pollEvents(bool& is_running);
    uint16_t getKeyInputRegister() const;

private:
    uint16_t key_input_state;
};