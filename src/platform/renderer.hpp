#pragma once
#include <SDL.h>
#include <cstdint>

class Renderer {
public:
    Renderer();
    ~Renderer();

    bool init(const char* title, int scale = 3);
    void renderFrame(const uint32_t* framebuffer);
    void clear();

private:
    SDL_Window* window;
    SDL_Renderer* sdl_renderer;
    SDL_Texture* texture;
    
    static const int GBA_WIDTH = 240;
    static const int GBA_HEIGHT = 160;
};