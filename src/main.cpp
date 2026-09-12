#include <iostream>
#include <iomanip>
#include <memory>
#include "Zenith/VirtualMachine.h"
#include "Zenith/OpcodeUtils.h" 

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
    auto stack = std::make_unique<Zenith::Stack>();
    Zenith::State state{ .stack = stack.get(), .pc = nullptr };
    Zenith::Machine vm;

    state.write_R32(1, int32_t(100));
    state.write_R32(2, int32_t(45));
    
    state.write_R32(3, 10.5f);
    state.write_R32(4, 2.5f);

    state.write_R64(1, int64_t(10000000000LL));
    state.write_R64(2, int64_t(4000000000LL));

    state.write_R64(3, 100.75);
    state.write_R64(4, 50.25);

    constexpr size_t instruction_count = 6;
    auto bytecode = std::make_unique<Zenith::Instruction[]>(instruction_count);
    bytecode[0] = Zenith::make_iABC(Zenith::Opcode::ADD_I32, 0, 1, 2);
    bytecode[1] = Zenith::make_iABC(Zenith::Opcode::SUB_F32, 5, 3, 4);
    bytecode[2] = Zenith::make_iABC(Zenith::Opcode::CVT_I32_F32, 6, 0, 0);
    bytecode[3] = Zenith::make_iABC(Zenith::Opcode::SUB_I64, 0, 1, 2);
    bytecode[4] = Zenith::make_iABC(Zenith::Opcode::ADD_F64, 5, 3, 4);
    bytecode[5] = Zenith::make_iABC(Zenith::Opcode::HALT, 0, 0, 0);

    state.pc = bytecode.get();

    std::cout << "[ BEFORE EXECUTION ]\n";
    dump_memory(*stack);

    vm.execute(state);

    std::cout << "\n[ AFTER EXECUTION ]\n";
    dump_memory(*stack);

    return 0;
}