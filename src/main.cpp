#include <iostream>
#include <iomanip>
#include "Zenith/VirtualMachine.h"

void dump_memory(const Zenith::Stack& stack, size_t count32 = 8, size_t count64 = 6)
{
    std::cout << "--- R32 BANK DUMP ---\n";
    for (size_t i = 0; i < count32; ++i)
    {
        uint32_t raw = stack.R32[i];
        int32_t i_val = std::bit_cast<int32_t>(raw);
        float f_val = std::bit_cast<float>(raw);
        
        std::cout << "R32[" << std::setw(2) << i << "] = HEX: 0x" 
                  << std::hex << std::setfill('0') << std::setw(8) << raw << std::dec
                  << " | INT: " << std::setw(8) << i_val
                  << " | FLOAT: " << f_val << "\n";
    }

    std::cout << "\n--- R64 BANK DUMP ---\n";
    for (size_t i = 0; i < count64; ++i)
    {
        uint64_t raw = stack.R64[i];
        int64_t i_val = std::bit_cast<int64_t>(raw);
        double d_val = std::bit_cast<double>(raw);
        
        std::cout << "R64[" << std::setw(2) << i << "] = HEX: 0x" 
                  << std::hex << std::setfill('0') << std::setw(16) << raw << std::dec
                  << " | INT64: " << std::setw(12) << i_val
                  << " | DOUBLE: " << d_val << "\n";
    }
}

int main()
{
    Zenith::Stack stack{};
    Zenith::State state{ .stack = &stack, .pc = nullptr };
    Zenith::Machine vm;

    state.write_R32(1, int32_t(100));
    state.write_R32(2, int32_t(45));
    
    state.write_R32(3, 10.5f);
    state.write_R32(4, 2.5f);

    state.write_R64(1, int64_t(10000000000LL));
    state.write_R64(2, int64_t(4000000000LL));

    state.write_R64(3, 100.75);
    state.write_R64(4, 50.25);

    Zenith::Instruction bytecode[] = {
        Zenith::make_inst(Zenith::Opcode::OP_ADD_I32, 0, 1, 2),
        Zenith::make_inst(Zenith::Opcode::OP_SUB_F32, 5, 3, 4),
        Zenith::make_inst(Zenith::Opcode::OP_CAST32_I2F, 6, 0, 0),

        Zenith::make_inst(Zenith::Opcode::OP_SUB_I64, 0, 1, 2),
        Zenith::make_inst(Zenith::Opcode::OP_ADD_F64, 5, 3, 4),
        
        Zenith::make_inst(Zenith::Opcode::OP_HALT)
    };

    state.pc = bytecode;

    std::cout << "[ BEFORE EXECUTION ]\n";
    dump_memory(stack);

    vm.execute(state);

    std::cout << "\n[ AFTER EXECUTION ]\n";
    dump_memory(stack);

    return 0;
}