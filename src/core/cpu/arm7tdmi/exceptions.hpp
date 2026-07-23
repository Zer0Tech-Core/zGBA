#pragma once

#include <cstdint>
#include "registers.hpp"

namespace zGBA::CPU::ARM7TDMI {

// Tipos de exceção com seus respectivos vetores de endereço[cite: 2]
enum class ExceptionType {
    Reset                = 0x00, //[cite: 2]
    UndefinedInstruction = 0x04, //[cite: 2]
    SoftwareInterrupt    = 0x08, //[cite: 2]
    PrefetchAbort        = 0x0C, //[cite: 2]
    DataAbort            = 0x10, //[cite: 2]
    IRQ                  = 0x18, //[cite: 2]
    FIQ                  = 0x1C  //[cite: 2]
};

class Exceptions {
public:
    // Dispara a rotina de exceção na CPU[cite: 2]
    static void raise(Registers& regs, ExceptionType type);

    // Processa o retorno de exceção quando MOVS/SUBS com destino PC é executado[cite: 2]
    static void restore_from_spsr(Registers& regs);
};

} // namespace zGBA::CPU::ARM7TDMI