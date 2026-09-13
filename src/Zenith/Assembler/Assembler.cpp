#include "Zenith/Assembler/Assembler.h"
#include <limits>
#include <algorithm>
#include <bit>
#include <cctype>
#include <climits>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "Zenith/Core/OpcodeUtils.h"

namespace Zenith {
namespace {


// ============================================================
// Opcode parser
// ============================================================

Opcode parse_opcode_string(const std::string& name)
{
    static const std::unordered_map<std::string, Opcode> op_map = {
        {"NOP",         Opcode::NOP},
        {"MOVE",        Opcode::MOVE},
        {"LOADI",       Opcode::LOADI},
        {"LOADF",       Opcode::LOADF},

        {"CVT_F64_I64", Opcode::CVT_F64_I64},
        {"ADD_I64",     Opcode::ADD_I64},
        {"SUB_I64",     Opcode::SUB_I64},
        {"MUL_I64",     Opcode::MUL_I64},
        {"DIV_I64",     Opcode::DIV_I64},

        {"CVT_I64_F64", Opcode::CVT_I64_F64},
        {"ADD_F64",     Opcode::ADD_F64},
        {"SUB_F64",     Opcode::SUB_F64},
        {"MUL_F64",     Opcode::MUL_F64},
        {"DIV_F64",     Opcode::DIV_F64},

        {"EQ_I64",      Opcode::EQ_I64},
        {"LT_I64",      Opcode::LT_I64},
        {"LE_I64",      Opcode::LE_I64},

        {"EQ_F64",      Opcode::EQ_F64},
        {"LT_F64",      Opcode::LT_F64},
        {"LE_F64",      Opcode::LE_F64},

        {"JMP",         Opcode::JMP},
        {"JMPIF",       Opcode::JMPIF},
        {"JMPIFNOT",   Opcode::JMPIFNOT},

        {"HALT",        Opcode::HALT}
    };

    auto it = op_map.find(name);

    if (it != op_map.end())
    {
        return it->second;
    }

    throw std::runtime_error("Invalid Opcode -> " + name);
}


// ============================================================
// Constant pool
// ============================================================

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

        uint32_t idx =
            static_cast<uint32_t>(values.size());

        values.push_back(raw);
        index_of.emplace(raw, idx);

        return idx;
    }

    uint32_t append(uint64_t raw)
    {
        return intern(raw);
    }

    const std::vector<uint64_t>& data() const
    {
        return values;
    }

private:
    std::vector<uint64_t> values;
    std::unordered_map<uint64_t, uint32_t> index_of;
};


// ============================================================
// Source representation
// ============================================================

struct ParsedLine
{
    size_t line_no = 0;
    std::string text;
    size_t pc = 0;
};

using LabelTable = std::unordered_map<std::string, size_t>;


// ============================================================
// Helpers
// ============================================================

std::string trim(const std::string& s)
{
    const char* whitespace = " \t\r\n";

    size_t begin =
        s.find_first_not_of(whitespace);

    if (begin == std::string::npos)
    {
        return {};
    }

    size_t end =
        s.find_last_not_of(whitespace);

    return s.substr(begin, end - begin + 1);
}


std::vector<std::string> tokenize(const std::string& line)
{
    std::vector<std::string> tokens;

    std::stringstream ss(line);
    std::string token;

    while (ss >> token)
    {
        tokens.push_back(token);
    }

    return tokens;
}


int parse_register(const std::string& token, size_t line_no)
{
    std::string value = token;
    if (!value.empty() && (value[0] == 'R' || value[0] == 'r')) value.erase(0, 1);
    if (value.empty())
    {
        throw std::runtime_error(
            "Line " + std::to_string(line_no) +
            ": invalid register '" + token + "'"
        );
    }

    size_t pos = 0;
    int reg = 0;

    try
    {
        reg = std::stoi(value, &pos);
    }
    catch (...)
    {
        throw std::runtime_error(
            "Line " + std::to_string(line_no) +
            ": invalid register '" + token + "'"
        );
    }

    if (pos != value.size() || reg < 0)
    {
        throw std::runtime_error(
            "Line " + std::to_string(line_no) +
            ": invalid register '" + token + "'"
        );
    }

    return reg;
}


template<typename T>
T parse_integer(const std::string& token, size_t line_no, const char* description)
{
    size_t pos = 0;
    try
    {
        long long value =
            std::stoll(token, &pos);

        if (pos != token.size())
        {
            throw std::runtime_error("");
        }

        return static_cast<T>(value);
    }
    catch (...)
    {
        throw std::runtime_error(
            "Line " + std::to_string(line_no) +
            ": invalid " + description +
            " '" + token + "'"
        );
    }
}


void update_register_count(int reg, uint32_t& register_count)
{
    const uint64_t required =
        static_cast<uint64_t>(reg) + 1;

    if (required >
        static_cast<uint64_t>(
            std::numeric_limits<uint32_t>::max()))
    {
        throw std::runtime_error(
            "Register index is too large"
        );
    }

    register_count =
        std::max(
            register_count,
            static_cast<uint32_t>(required)
        );
}


// ============================================================
// Remove comments
// ============================================================

std::string remove_comment(std::string line)
{
    size_t comment_pos =
        line.find_first_of(";#");

    if (comment_pos != std::string::npos)
    {
        line.resize(comment_pos);
    }

    return trim(line);
}


// ============================================================
// Label handling
// ============================================================

std::string extract_label(std::string& line, size_t line_no)
{
    line = trim(line);
    size_t colon = line.find(':');
    if (colon == std::string::npos) return {};

    std::string label = trim(line.substr(0, colon));
    if (label.empty())
    {
        throw std::runtime_error(
            "Line " + std::to_string(line_no) +
            ": empty label"
        );
    }

    for (char c : label)
    {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '.')
        {
            throw std::runtime_error(
                "Line " + std::to_string(line_no) +
                ": invalid label '" + label + "'"
            );
        }
    }
    line = trim(line.substr(colon + 1));
    return label;
}


// ============================================================
// First pass
// ============================================================

std::vector<ParsedLine> first_pass(
    const std::string& source,
    LabelTable& labels
)
{
    std::vector<ParsedLine> lines;

    std::stringstream ss(source);
    std::string raw_line;

    size_t line_no = 0;
    size_t pc = 0;

    while (std::getline(ss, raw_line))
    {
        ++line_no;

        std::string line =
            remove_comment(raw_line);

        if (line.empty())
        {
            continue;
        }

        std::string label =
            extract_label(line, line_no);

        if (!label.empty())
        {
            if (labels.contains(label))
            {
                throw std::runtime_error(
                    "Line " + std::to_string(line_no) +
                    ": duplicate label '" + label + "'"
                );
            }

            labels.emplace(label, pc);
        }

        if (line.empty())
        {
            continue;
        }

        if (line.starts_with(".CONST"))
        {
            continue;
        }

        lines.push_back({
            .line_no = line_no,
            .text = line,
            .pc = pc
        });

        ++pc;
    }

    return lines;
}


// ============================================================
// Assemble one instruction
// ============================================================

Instruction assemble_instruction(
    const ParsedLine& source_line,
    const LabelTable& labels,
    ConstantPool& pool,
    uint32_t& register_count
)
{
    const size_t line_no =
        source_line.line_no;

    std::vector<std::string> tokens =
        tokenize(source_line.text);

    if (tokens.empty())
    {
        throw std::runtime_error(
            "Line " + std::to_string(line_no) +
            ": empty instruction"
        );
    }

    const std::string& opcode_name =
        tokens[0];

    Opcode op;

    try
    {
        op = parse_opcode_string(opcode_name);
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error(
            "Line " + std::to_string(line_no) +
            ": " + e.what()
        );
    }

    const auto& info =
        get_opcode_info(op);


    // --------------------------------------------------------
    // iABC
    // --------------------------------------------------------

    if (info.mode == OpMode::iABC)
    {
        if (op == Opcode::NOP ||
            op == Opcode::HALT)
        {
            if (tokens.size() != 1)
            {
                throw std::runtime_error(
                    "Line " + std::to_string(line_no) +
                    ": " + opcode_name +
                    " takes no operands"
                );
            }

            return make_iABC(op, 0, 0, 0);
        }


        // MOVE A B

        if (op == Opcode::MOVE)
        {
            if (tokens.size() != 3)
            {
                throw std::runtime_error(
                    "Line " + std::to_string(line_no) +
                    ": MOVE expects A B"
                );
            }

            int a =
                parse_register(tokens[1], line_no);

            int b =
                parse_register(tokens[2], line_no);

            update_register_count(a, register_count);
            update_register_count(b, register_count);

            return make_iABC(op, a, b, 0);
        }


        // Conversion:
        //
        // CVT_F64_I64 A B
        // CVT_I64_F64 A B

        if (op == Opcode::CVT_F64_I64 ||
            op == Opcode::CVT_I64_F64)
        {
            if (tokens.size() != 3)
            {
                throw std::runtime_error(
                    "Line " + std::to_string(line_no) +
                    ": " + opcode_name +
                    " expects A B"
                );
            }

            int a =
                parse_register(tokens[1], line_no);

            int b =
                parse_register(tokens[2], line_no);

            update_register_count(a, register_count);
            update_register_count(b, register_count);

            return make_iABC(op, a, b, 0);
        }


        // Arithmetic / comparison:
        //
        // OP A B C

        if (op == Opcode::ADD_I64 ||
            op == Opcode::SUB_I64 ||
            op == Opcode::MUL_I64 ||
            op == Opcode::DIV_I64 ||

            op == Opcode::ADD_F64 ||
            op == Opcode::SUB_F64 ||
            op == Opcode::MUL_F64 ||
            op == Opcode::DIV_F64 ||

            op == Opcode::EQ_I64 ||
            op == Opcode::LT_I64 ||
            op == Opcode::LE_I64 ||

            op == Opcode::EQ_F64 ||
            op == Opcode::LT_F64 ||
            op == Opcode::LE_F64)
        {
            if (tokens.size() != 4)
            {
                throw std::runtime_error(
                    "Line " + std::to_string(line_no) +
                    ": " + opcode_name +
                    " expects A B C"
                );
            }

            int a =
                parse_register(tokens[1], line_no);

            int b =
                parse_register(tokens[2], line_no);

            int c =
                parse_register(tokens[3], line_no);

            update_register_count(a, register_count);
            update_register_count(b, register_count);
            update_register_count(c, register_count);

            return make_iABC(op, a, b, c);
        }
    }


    // --------------------------------------------------------
    // iABx
    // --------------------------------------------------------

    if (info.mode == OpMode::iABx)
    {
        if (tokens.size() != 3)
        {
            throw std::runtime_error(
                "Line " + std::to_string(line_no) +
                ": " + opcode_name +
                " expects A value"
            );
        }

        int a =
            parse_register(tokens[1], line_no);

        update_register_count(a, register_count);


        if (op == Opcode::LOADI)
        {
            int64_t value =
                parse_integer<int64_t>(
                    tokens[2],
                    line_no,
                    "I64 constant"
                );

            uint64_t raw =
                std::bit_cast<uint64_t>(value);

            uint32_t idx =
                pool.intern(raw);

            return make_iABx(op, a, idx);
        }


        if (op == Opcode::LOADF)
        {
            size_t pos = 0;
            double value = 0.0;

            try
            {
                value =
                    std::stod(tokens[2], &pos);

                if (pos != tokens[2].size())
                {
                    throw std::runtime_error("");
                }
            }
            catch (...)
            {
                throw std::runtime_error(
                    "Line " + std::to_string(line_no) +
                    ": invalid F64 constant '" +
                    tokens[2] + "'"
                );
            }

            uint64_t raw =
                std::bit_cast<uint64_t>(value);

            uint32_t idx =
                pool.intern(raw);

            return make_iABx(op, a, idx);
        }
    }


    // --------------------------------------------------------
    // iAsBx
    // --------------------------------------------------------

    if (info.mode == OpMode::iAsBx)
    {
        // JMP label

        if (op == Opcode::JMP)
        {
            if (tokens.size() != 2)
            {
                throw std::runtime_error(
                    "Line " + std::to_string(line_no) +
                    ": JMP expects label"
                );
            }

            const std::string& label =
                tokens[1];

            auto it = labels.find(label);

            if (it == labels.end())
            {
                throw std::runtime_error(
                    "Line " + std::to_string(line_no) +
                    ": unknown label '" + label + "'"
                );
            }

            const int64_t target_pc =
                static_cast<int64_t>(it->second);

            const int64_t current_pc =
                static_cast<int64_t>(
                    source_line.pc);

            const int64_t sbx =
                target_pc - (current_pc + 1);

            if (sbx < INT32_MIN ||
                sbx > INT32_MAX)
            {
                throw std::runtime_error(
                    "Line " + std::to_string(line_no) +
                    ": jump offset out of int32 range"
                );
            }

            return make_iAsBx(
                op,
                0,
                static_cast<int32_t>(sbx)
            );
        }


        // JMPIF Rcond label
        // JMPIFNOT Rcond label

        if (op == Opcode::JMPIF ||
            op == Opcode::JMPIFNOT)
        {
            if (tokens.size() != 3)
            {
                throw std::runtime_error(
                    "Line " + std::to_string(line_no) +
                    ": " + opcode_name +
                    " expects condition_register label"
                );
            }

            int a =
                parse_register(tokens[1], line_no);

            const std::string& label =
                tokens[2];

            auto it = labels.find(label);

            if (it == labels.end())
            {
                throw std::runtime_error(
                    "Line " + std::to_string(line_no) +
                    ": unknown label '" + label + "'"
                );
            }

            update_register_count(
                a,
                register_count
            );

            const int64_t target_pc =
                static_cast<int64_t>(it->second);

            const int64_t current_pc =
                static_cast<int64_t>(
                    source_line.pc);

            const int64_t sbx =
                target_pc - (current_pc + 1);

            if (sbx < INT32_MIN ||
                sbx > INT32_MAX)
            {
                throw std::runtime_error(
                    "Line " + std::to_string(line_no) +
                    ": jump offset out of int32 range"
                );
            }

            return make_iAsBx(
                op,
                a,
                static_cast<int32_t>(sbx)
            );
        }
    }


    throw std::runtime_error(
        "Line " + std::to_string(line_no) +
        ": unsupported instruction '" +
        opcode_name + "'"
    );
}
}


AssembledProgram
Assembler::assemble(const std::string& source) const
{
    AssembledProgram program;
    ConstantPool pool;
    LabelTable labels;

    std::vector<ParsedLine> instructions = first_pass(source, labels);
    std::stringstream ss(source);
    std::string raw_line;

    size_t line_no = 0;

    while (std::getline(ss, raw_line))
    {
        ++line_no;
        std::string line = remove_comment(raw_line);
        if (line.empty()) continue;

        extract_label(line, line_no);
        if (line.empty()) continue;

        std::vector<std::string> tokens = tokenize(line);
        if (tokens.empty()) continue;
        if (tokens[0] != ".CONST") continue;
        if (tokens.size() != 3) throw std::runtime_error("Line " + std::to_string(line_no) + ": .CONST expects type and value");

        const std::string& type_tag = tokens[1];
        if (type_tag == "I")
        {
            int64_t value = parse_integer<int64_t>(tokens[2], line_no, "I64 constant");
            pool.append(std::bit_cast<uint64_t>(value));
        }
        else if (type_tag == "F")
        {
            size_t pos = 0;
            double value = 0.0;

            try
            {
                value = std::stod(tokens[2], &pos);
                if (pos != tokens[2].size()) throw std::runtime_error("");
            }
            catch (...)
            {
                throw std::runtime_error("Line " + std::to_string(line_no) + ": invalid F64 constant '" + tokens[2] + "'");
            }
            pool.append(std::bit_cast<uint64_t>(value));
        }
        else throw std::runtime_error("Line " + std::to_string(line_no) + ": unknown .CONST type tag '" + type_tag +  "' (expected I or F)");
    }

    for (const ParsedLine& line : instructions)
        program.code.push_back(assemble_instruction(line, labels, pool, program.register_count));
    program.constants = pool.data();
    return program;
}

}