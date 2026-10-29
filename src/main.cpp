#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>

#include "Zenith/Compiler/Parser.h"
#include "Zenith/VM/Executor.h"
#include "Zenith/VM/Instruction.h"
#include "Zenith/VM/Opcode.h"
#include "Zenith/VM/Proto.h"
#include "Zenith/VM/State.h"

namespace fs = std::filesystem;

namespace {

    enum class Mode
    {
        None,
        Ast,
        Flat,
        Vm
    };

    void print_usage(const char* prog_name)
    {
        std::cout
            << "Usage: " << prog_name << " [file.zen] <mode>\n"
            << "Options:\n"
            << "  --ast         Dump S-expression AST to stdout\n"
            << "  --flat        Dump flat AST table to stdout\n"
            << "  --vm          Run hardcoded VM test proto\n"
            << "  -h, --help    Show this help message\n";
    }

    bool read_file(const fs::path& path, std::string& out_content)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file) return false;
        out_content.assign(
            std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>()
        );
        return true;
    }

}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        print_usage(argv[0]);
        return 1;
    }

    std::string file_path;
    Mode mode = Mode::None;

    for (int i = 1; i < argc; ++i)
    {
        std::string_view arg = argv[i];
        if (arg == "-h" || arg == "--help")
        {
            print_usage(argv[0]);
            return 0;
        }
        else if (arg == "--flat") mode = Mode::Flat;
        else if (arg == "--ast") mode = Mode::Ast;
        else if (arg == "--vm") mode = Mode::Vm;
        else if (!arg.empty() && arg[0] == '-')
        {
            std::cerr << "Error: Unknown option '" << arg << "'\n";
            print_usage(argv[0]);
            return 1;
        }
        else file_path = std::string(arg);
    }

    if (mode == Mode::None)
    {
        std::cerr << "Error: No execution mode specified (--ast, --flat, or --vm).\n";
        print_usage(argv[0]);
        return 1;
    }

    if (mode == Mode::Vm)
    {
        using namespace Zenith;

        Proto proto(0, 5);

        Value v10; v10.i32 = 10;
        Value v20; v20.i32 = 20;
        Value v3;  v3.i32  = 3;

        uint16_t c0 = proto.push_constant(v10);
        uint16_t c1 = proto.push_constant(v20);
        uint16_t c2 = proto.push_constant(v3);

        proto.commit_instruction(Instruction::make<Format::iABx>(Opcode::LoadConst, Type::I32, 0, c0));
        proto.commit_instruction(Instruction::make<Format::iABx>(Opcode::LoadConst, Type::I32, 1, c1));
        proto.commit_instruction(Instruction::make<Format::iABC>(Opcode::Add, Type::I32, 2, 0, 1));
        proto.commit_instruction(Instruction::make<Format::iABx>(Opcode::LoadConst, Type::I32, 3, c2));
        proto.commit_instruction(Instruction::make<Format::iABC>(Opcode::Neg, Type::I32, 3, 3, 0));
        proto.commit_instruction(Instruction::make<Format::iABC>(Opcode::Mul, Type::I32, 4, 2, 3));
        proto.commit_instruction(Instruction::make<Format::iNone>(Opcode::Halt));

        State state;
        uint16_t id = state.add_proto(std::move(proto));
        state.prepare(id);

        Executor::execute(state);

        std::cout << state.reg(4).i32 << "\n";
        std::cout.flush();
        return 0;
    }

    if (file_path.empty())
    {
        std::cerr << "Error: No input file specified.\n";
        print_usage(argv[0]);
        return 1;
    }

    std::string source;
    if (!read_file(file_path, source))
    {
        std::cerr << "Error: Could not open file '" << file_path << "'\n";
        return 1;
    }

    Zenith::Parser parser(source);
    uint32_t root = parser.parse_program();

    if (parser.has_errors())
    {
        parser.dump_errors(std::cerr);
        return 1;
    }

    if (mode == Mode::Flat)
    {
        parser.dump_flat_ast(std::cout);
    }
    else if (mode == Mode::Ast)
    {
        parser.dump_tree_ast(std::cout, root);
    }

    std::cout.flush();
    return 0;
}
