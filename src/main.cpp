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

        Proto proto(0, 8);

        proto.commit_instruction(Instruction::make<Format::iAsBx>(Opcode::LoadImm, Type::None, 0, 27));
        proto.commit_instruction(Instruction::make<Format::iAsBx>(Opcode::LoadImm, Type::None, 1, 0));
        proto.commit_instruction(Instruction::make<Format::iAsBx>(Opcode::LoadImm, Type::None, 2, 1));
        proto.commit_instruction(Instruction::make<Format::iAsBx>(Opcode::LoadImm, Type::None, 3, 2));
        proto.commit_instruction(Instruction::make<Format::iAsBx>(Opcode::LoadImm, Type::None, 4, 3));

        uint32_t loop_start = proto.commit_instruction(
            Instruction::make<Format::iABC>(Opcode::Eq, Type::I32, 5, 0, 2)
        );
        uint32_t exit_jmp = proto.commit_instruction(
            Instruction::make<Format::iAsBx>(Opcode::Jnz, 5, 0)
        );

        proto.commit_instruction(Instruction::make<Format::iABC>(Opcode::RemS, Type::I32, 5, 0, 3));
        proto.commit_instruction(Instruction::make<Format::iABC>(Opcode::LNot, Type::None, 6, 5, 0));

        uint32_t odd_jmp = proto.commit_instruction(
            Instruction::make<Format::iAsBx>(Opcode::Jz, 6, 0)
        );

        proto.commit_instruction(Instruction::make<Format::iABC>(Opcode::DivU, Type::U32, 0, 0, 3));

        uint32_t continue_jmp = proto.commit_instruction(
            Instruction::make<Format::iAsBx>(Opcode::Jmp, 0, 0)
        );

        uint32_t odd_case = proto.commit_instruction(
            Instruction::make<Format::iABC>(Opcode::Mul, Type::I32, 5, 0, 4)
        );
        proto.commit_instruction(Instruction::make<Format::iABC>(Opcode::Add, Type::I32, 0, 5, 2));

        uint32_t continue_label = proto.commit_instruction(
            Instruction::make<Format::iABC>(Opcode::Add, Type::I32, 1, 1, 2)
        );

        uint32_t loop_back = proto.commit_instruction(
            Instruction::make<Format::iAsBx>(Opcode::Jmp, 0, 0)
        );
        proto.patch_jmp(loop_back, static_cast<int16_t>(int32_t(loop_start) - int32_t(loop_back + 1)));

        uint32_t done_label = proto.commit_instruction(
            Instruction::make<Format::iNone>(Opcode::Halt)
        );

        proto.patch_jmp(exit_jmp,     static_cast<int16_t>(int32_t(done_label)     - int32_t(exit_jmp + 1)));
        proto.patch_jmp(odd_jmp,      static_cast<int16_t>(int32_t(odd_case)       - int32_t(odd_jmp + 1)));
        proto.patch_jmp(continue_jmp, static_cast<int16_t>(int32_t(continue_label) - int32_t(continue_jmp + 1)));

        State state;
        uint16_t id = state.add_proto(std::move(proto));
        state.prepare(id);

        Executor::execute(state);

        std::cout << "Collatz(27): " << state.reg(1).i32
                << " steps (n=" << state.reg(0).i32 << ")\n";
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
