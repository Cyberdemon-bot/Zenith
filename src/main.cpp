#include <bit>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

#include "Zenith/Assembler/Assembler.h"
#include "Zenith/Core/VirtualMachine.h"


std::string read_file(const std::string& filepath)
{
    std::ifstream file(filepath);

    if (!file.is_open())
    {
        throw std::runtime_error(
            "Cannot open script file: " + filepath
        );
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    return buffer.str();
}

void dump_memory(const Zenith::Stack& stack, uint32_t register_count)
{
    std::cout << "\n--- R64 BANK DUMP ---\n";
    for (uint32_t i = 0; i < register_count; ++i)
    {
        uint64_t raw = stack.read<uint64_t>(i);
        int64_t i_val = std::bit_cast<int64_t>(raw);
        double d_val = std::bit_cast<double>(raw);
        std::cout
            << "R64[" << std::dec << std::setfill('0') << std::setw(2) << i << "] = HEX: 0x"
            << std::hex << std::setfill('0') << std::setw(16) << raw << std::dec << std::setfill(' ')
            << " | I64: " << std::setw(20) << i_val
            << " | F64: " << std::setw(22) << std::left << d_val << std::right
            << "\n";
    }
}


void dump_constants(const std::vector<uint64_t>& constants)
{
    std::cout << "\n--- CONSTANT POOL (" << constants.size() << " entries) ---\n";
    for (size_t i = 0; i < constants.size(); ++i)
    {
        uint64_t raw = constants[i];
        int64_t i_val = std::bit_cast<int64_t>(raw);
        double d_val = std::bit_cast<double>(raw);
        std::cout 
            << "K[" << std::dec << std::setfill('0') << std::setw(2) << i << "] = HEX: 0x"
            << std::hex << std::setfill('0') << std::setw(16) << raw << std::dec << std::setfill(' ')
            << " | I64: " << std::setw(20) << i_val
            << " | F64: " << std::setw(22) << std::left << d_val << std::right
            << "\n";
    }
}


int main(int argc, char* argv[])
{
    std::string filename = "";
    if (argc > 1) filename = argv[1];
    if (filename.empty()) return 0;
  
    auto stack = std::make_unique<Zenith::Stack>();

    Zenith::State state{ .stack = stack.get() };

    Zenith::Machine vm;
    std::cout << "Reading script: " << filename << "...\n\n";
    std::string source = read_file(filename);
    Zenith::Assembler assembler;
    Zenith::AssembledProgram program = assembler.assemble(source);
    state.proto = Zenith::Proto
    {
        .code = program.code,
        .numeric_constants = program.constants,
        .register_count = program.register_count
    };


    std::cout << "Code size       : " << state.proto.code.size() << "\n";
    std::cout << "Constant count  : " << state.proto.numeric_constants.size() << "\n";
    std::cout << "Register count  : " << state.proto.register_count << "\n";
    dump_constants(program.constants);
    std::cout << "\n[ BEFORE EXECUTION ]\n";
    dump_memory(*stack, state.proto.register_count);
    vm.execute(state);
    std::cout << "\n[ AFTER EXECUTION ]\n";
    dump_memory(*stack, state.proto.register_count);
    return 0;
}
