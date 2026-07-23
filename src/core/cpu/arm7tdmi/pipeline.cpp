#include "pipeline.hpp"

namespace zGBA::CPU::ARM7TDMI {

void Pipeline::flush(Registers& regs, const MemoryRead32& read32, const MemoryRead16& read16) {
    fetch_stage.valid = false;
    decode_stage.valid = false;
    execute_stage.valid = false;

    // Enche o pipeline com os dois primeiros estágios (Fetch e Decode)[cite: 2]
    // 1º Ciclo de Busca (Fetch)
    uint32_t pc = regs.get_pc();
    if (regs.is_thumb()) {
        decode_stage.instruction = read16(pc & ~1u);
        decode_stage.address = pc;
        decode_stage.valid = true;
        regs.set_pc(pc + 2); // PC avança em Thumb[cite: 2]
    } else {
        decode_stage.instruction = read32(pc & ~3u);
        decode_stage.address = pc;
        decode_stage.valid = true;
        regs.set_pc(pc + 4); // PC avança em ARM[cite: 2]
    }

    // 2º Ciclo de Busca (Passa o primeiro para Decode e busca a segunda instrução)
    pc = regs.get_pc();
    if (regs.is_thumb()) {
        fetch_stage.instruction = read16(pc & ~1u);
        fetch_stage.address = pc;
        fetch_stage.valid = true;
        regs.set_pc(pc + 2); //[cite: 2]
    } else {
        fetch_stage.instruction = read32(pc & ~3u);
        fetch_stage.address = pc;
        fetch_stage.valid = true;
        regs.set_pc(pc + 4); //[cite: 2]
    }
}

PipelineStage Pipeline::step(Registers& regs, const MemoryRead32& read32, const MemoryRead16& read16) {
    // 1. Move a instrução Decodificada para o Estágio de Execução[cite: 2]
    execute_stage = decode_stage;

    // 2. Move a instrução Buscada para o Estágio de Decodificação[cite: 2]
    decode_stage = fetch_stage;

    // 3. Executa um novo Fetch da memória na posição atual do PC[cite: 2]
    uint32_t pc = regs.get_pc();
    fetch_stage.address = pc;
    fetch_stage.valid = true;

    if (regs.is_thumb()) {
        fetch_stage.instruction = read16(pc & ~1u);
        regs.set_pc(pc + 2); // PC em execução fica sempre +4 do instrução atual em Thumb[cite: 2]
    } else {
        fetch_stage.instruction = read32(pc & ~3u);
        regs.set_pc(pc + 4); // PC em execução fica sempre +8 do instrução atual em ARM[cite: 2]
    }

    return execute_stage;
}

} // namespace zGBA::CPU::ARM7TDMI