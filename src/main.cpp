#include <iostream>
#include <fstream>
#include <iomanip>
#include <memory>
#include <vector>
#include <string>
#include <sstream>
#include <unordered_map>
#include <stdexcept>
#include <cstdint>
#include <bit>

#include "Zenith/VirtualMachine.h"
#include "Zenith/Core/OpcodeUtils.h"

Zenith::Opcode parse_opcode_string(const std::string& name)
{
    static const std::unordered_map<std::string, Zenith::Opcode> op_map = {
        {"NOP",         Zenith::Opcode::NOP},
        {"MOVE",        Zenith::Opcode::MOVE},
        {"LOADI",       Zenith::Opcode::LOADI},
        {"LOADF",       Zenith::Opcode::LOADF},

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


class ConstantPool
{
public:
    uint32_t intern(uint64_t raw)
    {
        auto it = index_of.find(raw);
        if (it != index_of.end())
        {
            return it->second;
        }
        uint32_t idx = static_cast<uint32_t>(values.size());
        values.push_back(raw);
        index_of.emplace(raw, idx);
        return idx;
    }

    uint32_t append(uint64_t raw)
    {
        return intern(raw);
    }

    const std::vector<uint64_t>& data() const { return values; }

private:
    std::vector<uint64_t> values;
    std::unordered_map<uint64_t, uint32_t> index_of;
};

struct AssembledProgram
{
    std::vector<Zenith::Instruction> code;
    std::vector<uint64_t> constants;
};

AssembledProgram assemble(const std::string& source)
{
    AssembledProgram program;
    ConstantPool pool;

    std::stringstream ss(source);
    std::string line;
    size_t line_no = 0;

    while (std::getline(ss, line))
    {
        ++line_no;

        size_t comment_pos = line.find_first_of(";#");
        if (comment_pos != std::string::npos)
        {
            line = line.substr(0, comment_pos);
        }

        std::stringstream line_stream(line);
        std::string token;
        if (!(line_stream >> token)) continue;

        if (token == ".CONST")
        {
            std::string type_tag;
            if (!(line_stream >> type_tag))
            {
                throw std::runtime_error(
                    "Line " + std::to_string(line_no) + ": .CONST requires a type tag (I or F) and a value");
            }

            uint64_t raw = 0;
            if (type_tag == "I")
            {
                int64_t v = 0;
                if (!(line_stream >> v))
                    throw std::runtime_error("Line " + std::to_string(line_no) + ": .CONST I expects an integer value");
                raw = std::bit_cast<uint64_t>(v);
            }
            else if (type_tag == "F")
            {
                double v = 0.0;
                if (!(line_stream >> v))
                    throw std::runtime_error("Line " + std::to_string(line_no) + ": .CONST F expects a float value");
                raw = std::bit_cast<uint64_t>(v);
            }
            else
            {
                throw std::runtime_error(
                    "Line " + std::to_string(line_no) + ": unknown .CONST type tag '" + type_tag + "' (expected I or F)");
            }

            pool.append(raw);
            continue;
        }

        Zenith::Opcode op = parse_opcode_string(token);
        const auto& info = Zenith::get_opcode_info(op);

        if (info.mode == Zenith::OpMode::iABC)
        {
            int a = 0, b = 0, c = 0;
            line_stream >> a >> b >> c;
            program.code.push_back(Zenith::make_iABC(op, a, b, c));
        }
        else if (info.mode == Zenith::OpMode::iAsBx)
        {
            int a = 0;
            int32_t sbx = 0;
            line_stream >> a >> sbx;
            program.code.push_back(Zenith::make_iAsBx(op, a, sbx));
        }
        else if (info.mode == Zenith::OpMode::iABx)
        {
            int a = 0;

            if (op == Zenith::Opcode::LOADI)
            {
                int64_t value = 0;
                line_stream >> a >> value;
                uint64_t raw = std::bit_cast<uint64_t>(value);
                uint32_t idx = pool.intern(raw);
                program.code.push_back(Zenith::make_iABx(op, a, idx));
            }
            else if (op == Zenith::Opcode::LOADF)
            {
                double value = 0.0;
                line_stream >> a >> value;
                uint64_t raw = std::bit_cast<uint64_t>(value);
                uint32_t idx = pool.intern(raw);
                program.code.push_back(Zenith::make_iABx(op, a, idx));
            }
            else
            {
                uint32_t bx = 0;
                line_stream >> a >> bx;
                program.code.push_back(Zenith::make_iABx(op, a, bx));
            }
        }
    }

    program.constants = pool.data();
    return program;
}

void dump_memory(const Zenith::Stack& stack, size_t count64 = 16)
{
    std::cout << "\n--- R64 BANK DUMP ---\n";
    for (size_t i = 0; i < count64; ++i)
    {
        uint64_t raw = stack.read<uint64_t>(i);
        int64_t i_val = std::bit_cast<int64_t>(raw);
        double d_val = std::bit_cast<double>(raw);

        std::cout << "R64[" << std::dec << std::setfill('0') << std::setw(2) << i << "] = HEX: 0x"
                  << std::hex << std::setfill('0') << std::setw(16) << raw
                  << std::dec << std::setfill(' ')
                  << " | I64: " << std::setw(20) << i_val
                  << " | F64: " << std::setw(22) << std::left << d_val << std::right << "\n";
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

        std::cout << "K[" << std::dec << std::setfill('0') << std::setw(2) << i << "] = HEX: 0x"
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
    Zenith::State state{ .stack = stack.get() };
    Zenith::Machine vm;

    state.stack->write(1, int64_t(10000000000LL));
    state.stack->write(2, int64_t(2000000000LL));

    std::cout << "Reading script: " << filename << "...\n\n";
    std::string source = read_file(filename);

    AssembledProgram program = assemble(source);
    state.proto = Zenith::Proto{
        .code = program.code,
        .numeric_constants = program.constants
    };

    dump_constants(program.constants);

    std::cout << "[ BEFORE EXECUTION ]\n";
    dump_memory(*stack, 30);

    vm.execute(state);

    std::cout << "\n[ AFTER EXECUTION ]\n";
    dump_memory(*stack, 30);

    return 0;
}