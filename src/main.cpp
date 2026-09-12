#include <iostream>
#include <fstream>
#include <iomanip>
#include <memory>
#include <vector>
#include <string>
#include <sstream>
#include <unordered_map>
#include <stdexcept>

#include "Zenith/VirtualMachine.h"
#include "Zenith/OpcodeUtils.h" 

Zenith::Opcode parse_opcode_string(const std::string& name)
{
    static const std::unordered_map<std::string, Zenith::Opcode> op_map = {
        {"NOP",         Zenith::Opcode::NOP},
        {"MOVE",        Zenith::Opcode::MOVE},
        {"LOADI",       Zenith::Opcode::LOADI},

        {"CVT_F32_I32", Zenith::Opcode::CVT_F32_I32},
        {"ADD_I32",     Zenith::Opcode::ADD_I32},
        {"SUB_I32",     Zenith::Opcode::SUB_I32},
        {"MUL_I32",     Zenith::Opcode::MUL_I32},
        {"DIV_I32",     Zenith::Opcode::DIV_I32},
        {"MOD_I32",     Zenith::Opcode::MOD_I32},

        {"CVT_I32_F32", Zenith::Opcode::CVT_I32_F32},
        {"ADD_F32",     Zenith::Opcode::ADD_F32},
        {"SUB_F32",     Zenith::Opcode::SUB_F32},
        {"MUL_F32",     Zenith::Opcode::MUL_F32},
        {"DIV_F32",     Zenith::Opcode::DIV_F32},

        {"CVT_F64_I64", Zenith::Opcode::CVT_F64_I64},
        {"ADD_I64",     Zenith::Opcode::ADD_I64},
        {"SUB_I64",     Zenith::Opcode::SUB_I64},
        {"MUL_I64",     Zenith::Opcode::MUL_I64},
        {"DIV_I64",     Zenith::Opcode::DIV_I64},

        {"CVT_I64_F64", Zenith::Opcode::CVT_I64_F64},
        {"ADD_F64",     Zenith::Opcode::ADD_F64},
        {"SUB_F64",     Zenith::Opcode::SUB_F64},
        {"MUL_F64",     Zenith::Opcode::MUL_F64},
        {"DIV_F64",     Zenith::Opcode::DIV_F64},

        {"HALT",        Zenith::Opcode::HALT}
    };

    auto it = op_map.find(name);
    if (it != op_map.end())
    {
        return it->second;
    }
    throw std::runtime_error("Invalid Opcode -> " + name);
}

std::string read_file(const std::string& filepath)
{
    std::ifstream file(filepath);
    if (!file.is_open())
    {
        throw std::runtime_error("Cannot open script file: " + filepath);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::vector<Zenith::Instruction> assemble(const std::string& source)
{
    std::vector<Zenith::Instruction> bytecode;
    std::stringstream ss(source);
    std::string line;

    while (std::getline(ss, line))
    {
        size_t comment_pos = line.find_first_of(";#");
        if (comment_pos != std::string::npos)
        {
            line = line.substr(0, comment_pos);
        }

        std::stringstream line_stream(line);
        std::string op_str;
        if (!(line_stream >> op_str)) continue;

        Zenith::Opcode op = parse_opcode_string(op_str);
        const auto& info = Zenith::get_opcode_info(op);

        if (info.mode == Zenith::OpMode::iABC)
        {
            int a = 0, b = 0, c = 0;
            line_stream >> a >> b >> c;
            bytecode.push_back(Zenith::make_iABC(op, a, b, c));
        }
        else if (info.mode == Zenith::OpMode::iAsBx)
        {
            int a = 0;
            int32_t sbx = 0;
            line_stream >> a >> sbx;
            bytecode.push_back(Zenith::make_iAsBx(op, a, sbx));
        }
        else if (info.mode == Zenith::OpMode::iABx)
        {
            int a = 0;
            uint32_t bx = 0;
            line_stream >> a >> bx;
            bytecode.push_back(Zenith::make_iABx(op, a, bx));
        }
    }

    return bytecode;
}

void dump_memory(const Zenith::Stack& stack, size_t count32 = 16, size_t count64 = 16)
{
    std::cout << "--- R32 BANK DUMP ---\n";
    for (size_t i = 0; i < count32; ++i)
    {
        uint32_t raw = stack.R32[i];
        int32_t i_val = std::bit_cast<int32_t>(raw);
        float f_val = std::bit_cast<float>(raw);
        
        std::cout << "R32[" << std::dec << std::setfill('0') << std::setw(2) << i << "] = HEX: 0x" 
                  << std::hex << std::setfill('0') << std::setw(8) << raw
                  << std::dec << std::setfill(' ')
                  << " | I32: " << std::setw(11) << i_val
                  << " | F32: " << std::setw(14) << std::left << f_val << std::right << "\n";
    }

    std::cout << "\n--- R64 BANK DUMP ---\n";
    for (size_t i = 0; i < count64; ++i)
    {
        uint64_t raw = stack.R64[i];
        int64_t i_val = std::bit_cast<int64_t>(raw);
        double d_val = std::bit_cast<double>(raw);
        
        std::cout << "R64[" << std::dec << std::setfill('0') << std::setw(2) << i << "] = HEX: 0x" 
                  << std::hex << std::setfill('0') << std::setw(16) << raw
                  << std::dec << std::setfill(' ')
                  << " | I64: " << std::setw(20) << i_val
                  << " | F64: " << std::setw(22) << std::left << d_val << std::right << "\n";
    }
}

int main(int argc, char* argv[])
{
    std::string filename = "script/test.zasm";
    if (argc > 1)
    {
        filename = argv[1];
    }

    auto stack = std::make_unique<Zenith::Stack>();
    Zenith::State state{ .stack = stack.get(), .pc = nullptr };
    Zenith::Machine vm;

    state.write_R64(1, int64_t(10000000000LL));
    state.write_R64(2, int64_t(2000000000LL));

    try 
    {
        std::cout << "Reading script: " << filename << "...\n\n";
        std::string source = read_file(filename);

        std::vector<Zenith::Instruction> bytecode = assemble(source);
        state.pc = bytecode.data();

        std::cout << "[ BEFORE EXECUTION ]\n";
        dump_memory(*stack, 16, 16);

        vm.execute(state);

        std::cout << "\n[ AFTER EXECUTION ]\n";
        dump_memory(*stack, 16, 16);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Loi: " << e.what() << "\n";
        return 1;
    }

    return 0;
}