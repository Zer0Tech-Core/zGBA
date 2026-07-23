#pragma once
#include <cstdint>

namespace zgba::debug {
class EmbeddedICE {
public:
    EmbeddedICE() = default;
    ~EmbeddedICE() = default;
    // Registradores de controle de debug do ARM7TDMI (Watchpoints/Breakpoints de hardware)
};
} // namespace zgba::debug