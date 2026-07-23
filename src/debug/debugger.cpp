#include "debugger.hpp"
#include <iostream>
#include <iomanip>
#include <string>
#include <cstdlib>

namespace zgba::debug {

Debugger::Debugger(zGBA::CPU::ARM7TDMI::CPU& cpu) 
    : cpu_ref(cpu), enabled(false) {}

void Debugger::add_breakpoint(uint32_t address) {
    breakpoints.insert(address);
    std::cout << "[DEBUGGER] Breakpoint adicionado em 0x" << std::hex << address << std::dec << std::endl;
}

void Debugger::remove_breakpoint(uint32_t address) {
    breakpoints.erase(address);
}

bool Debugger::check_breakpoint(uint32_t address) const {
    return breakpoints.find(address) != breakpoints.end();
}

void Debugger::set_enabled(bool e) {
    enabled = e;
}

bool Debugger::is_enabled() const {
    return enabled;
}

void Debugger::print_state() const {
    const auto& regs = cpu_ref.get_registers();
    std::cout << "----------------------------------------\n";
    std::cout << "PC: 0x" << std::hex << std::setw(8) << std::setfill('0') << regs.get_pc() << "\n";
    std::cout << "----------------------------------------\n" << std::dec;
}

void Debugger::interactive_prompt() {
    std::string command;
    std::cout << "[DEBUGGER] Modo interativo ativado. Digite 'help' ou comandos.\n";
    
    bool interactive_active = true;
    while (interactive_active) {
        std::cout << "(zgba-cli) ";
        if (!(std::cin >> command)) break;

        if (command == "c" || command == "continue") {
            interactive_active = false; 
        } 
        else if (command == "s" || command == "step") {
            cpu_ref.step();
            print_state();
        } 
        else if (command == "regs" || command == "registers") {
            print_state();
        } 
        else if (command == "b" || command == "break") {
            uint32_t addr;
            std::cin >> std::hex >> addr;
            add_breakpoint(addr);
        } 
        else if (command == "quit" || command == "exit") {
            std::exit(0);
        } 
        else {
            std::cout << "Comandos: c (continue), s (step), regs, b (break <hex>), quit\n";
        }
    }
}

void Debugger::update() {
    if (!enabled) return;
    uint32_t current_pc = cpu_ref.get_registers().get_pc();
    if (check_breakpoint(current_pc)) {
        std::cout << "[DEBUGGER] Breakpoint atingido em 0x" << std::hex << current_pc << std::dec << "!\n";
        print_state();
        interactive_prompt();
    }
}

} // namespace zgba::debug