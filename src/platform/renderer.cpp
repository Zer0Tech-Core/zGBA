#include "renderer.hpp"
#include <iostream>

Renderer::Renderer() : window(nullptr), sdl_renderer(nullptr), texture(nullptr) {}

Renderer::~Renderer() {
    if (texture) SDL_DestroyTexture(texture);
    if (sdl_renderer) SDL_DestroyRenderer(sdl_renderer);
    if (window) SDL_DestroyWindow(window);
}

bool Renderer::init(const char* title, int scale) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Erro ao inicializar SDL Video: " << SDL_GetError() << std::endl;
        return false;
    }

    window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              GBA_WIDTH * scale, GBA_HEIGHT * scale, SDL_WINDOW_SHOWN);
    if (!window) {
        std::cerr << "Erro ao criar janela SDL: " << SDL_GetError() << std::endl;
        return false;
    }

    sdl_renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!sdl_renderer) {
        std::cerr << "Erro ao criar renderizador SDL: " << SDL_GetError() << std::endl;
        return false;
    }

    texture = SDL_CreateTexture(sdl_renderer, SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING, GBA_WIDTH, GBA_HEIGHT);
    if (!texture) {
        std::cerr << "Erro ao criar textura SDL: " << SDL_GetError() << std::endl;
        return false;
    }

    return true;
}

void Renderer::renderFrame(const uint32_t* framebuffer) {
    SDL_UpdateTexture(texture, nullptr, framebuffer, GBA_WIDTH * sizeof(uint32_t));
    SDL_RenderClear(sdl_renderer);
    SDL_RenderCopy(sdl_renderer, texture, nullptr, nullptr);
    SDL_RenderPresent(sdl_renderer);
}

void Renderer::clear() {
    if (sdl_renderer) {
        SDL_SetRenderDrawColor(sdl_renderer, 0, 0, 0, 255);
        SDL_RenderClear(sdl_renderer);
        SDL_RenderPresent(sdl_renderer);
    }
}