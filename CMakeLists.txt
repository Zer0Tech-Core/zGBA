cmake_minimum_required(VERSION 3.16)
project(zGBA LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra)

include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src)

# Fontes do Núcleo da CPU ARM7TDMI
set(CPU_SOURCES
    src/core/cpu/arm7tdmi/registers.cpp
    src/core/cpu/arm7tdmi/exceptions.cpp
    src/core/cpu/arm7tdmi/pipeline.cpp
    src/core/cpu/cpu.cpp
    src/core/cpu/thumb/thumb_decoder.cpp
)

# Fontes do Barramento e Sistema de Memória
set(MEMORY_SOURCES
    src/core/memory/bus.cpp
    src/core/memory/memory_map.cpp
)

# Fontes do Subsistema de Vídeo (PPU)
set(VIDEO_SOURCES
    src/core/video/ppu.cpp
)

# Fontes do Subsistema de Áudio (APU e Mixer)
set(AUDIO_SOURCES
    src/core/audio/apu.cpp
    src/core/audio/mixer.cpp
)

# Fontes da Camada Platform
set(PLATFORM_SOURCES
    src/platform/rom_loader.cpp
    src/platform/input.cpp
    src/platform/renderer.cpp
    src/platform/audio_backend.cpp
    src/platform/emulador.cpp
)

# Fontes do Subsistema de Debug
set(DEBUG_SOURCES
    src/debug/debugger.cpp
)

# ------------------------------------------------------------------------------
# Encontrar e vincular a biblioteca SDL2 (necessária para Renderer, Input e o Executável)
# ------------------------------------------------------------------------------
find_package(SDL2 REQUIRED)


# ------------------------------------------------------------------------------
# Executável Principal do zGBA
# ------------------------------------------------------------------------------
add_executable(zgba
    ${CPU_SOURCES}
    ${MEMORY_SOURCES}
    ${VIDEO_SOURCES}
    ${AUDIO_SOURCES}
    ${PLATFORM_SOURCES}
    ${DEBUG_SOURCES}
    src/platform/main.cpp
)

target_link_libraries(zgba PRIVATE SDL2::SDL2)

# ------------------------------------------------------------------------------
# Executáveis de Teste
# ------------------------------------------------------------------------------
add_executable(test_cpu
    ${CPU_SOURCES}
    tests/test_cpu_core.cpp
)

add_executable(test_memory
    ${CPU_SOURCES}
    ${MEMORY_SOURCES}
    tests/test_memory.cpp
)

add_executable(test_video
    ${CPU_SOURCES}
    ${MEMORY_SOURCES}
    ${VIDEO_SOURCES}
    tests/test_video.cpp
)

add_executable(test_audio
    ${CPU_SOURCES}
    ${MEMORY_SOURCES}
    ${AUDIO_SOURCES}
    tests/test_audio.cpp
)

# Executável de Teste da Plataforma
add_executable(test_platform
    ${CPU_SOURCES}
    ${MEMORY_SOURCES}
    ${VIDEO_SOURCES}
    ${AUDIO_SOURCES}
    ${PLATFORM_SOURCES}
    ${DEBUG_SOURCES} # <--- Adicione esta linha aqui
    tests/test_platform.cpp
)

target_link_libraries(test_platform PRIVATE SDL2::SDL2)

# ------------------------------------------------------------------------------
# Configuração do CTest
# ------------------------------------------------------------------------------
enable_testing()

add_test(NAME CPU_Core_Tests COMMAND test_cpu)
add_test(NAME Memory_Tests COMMAND test_memory)
add_test(NAME Video_Tests COMMAND test_video)
add_test(NAME Audio_Tests COMMAND test_audio)
add_test(NAME Platform_Tests COMMAND test_platform)

# ------------------------------------------------------------------------------
# Alvos Customizados ('make cpu', 'make mem', 'make video', 'make audio')
# ------------------------------------------------------------------------------
add_custom_target(cpu
    COMMAND test_cpu
    DEPENDS test_cpu
    COMMENT ">>> Executando testes da CPU (ARM7TDMI / Thumb)..."
)

add_custom_target(mem
    COMMAND test_memory
    DEPENDS test_memory
    COMMENT ">>> Executando testes do Sistema de Memória e Integração..."
)

add_custom_target(video
    COMMAND test_video
    DEPENDS test_video
    COMMENT ">>> Executando testes do Subsistema de Vídeo (PPU)..."
)

add_custom_target(audio
    COMMAND test_audio
    DEPENDS test_audio
    COMMENT ">>> Executando testes do Subsistema de Áudio (APU / Direct Sound)..."
)

# Compila tudo e roda a suíte de testes do CTest com log de falhas em tempo real
add_custom_target(test_runner
    ALL
    DEPENDS test_cpu test_memory test_video test_audio test_platform
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    COMMENT ">>> Executando toda a suíte de testes do zGBA..."
)

# Alvo customizado para execução direta via 'make platform_test'
add_custom_target(platform_test
    COMMAND test_platform
    DEPENDS test_platform
    COMMENT ">>> Executando testes unitarios e integrados da camada Platform..."
)