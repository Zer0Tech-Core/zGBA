#include "input.hpp"

Input::Input() : key_input_state(0x03FF) {} // Todos os botões soltos (lógica invertida)

void Input::pollEvents(bool& is_running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            is_running = false;
        }
    }
}

uint16_t Input::getKeyInputRegister() const {
    return key_input_state;
}