#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>

#include "Zenith/Compiler/Parser.h"
#include "Zenith/VM/Assembler.h"
#include "Zenith/VM/Disassembler.h"
#include "Zenith/VM/Executor.h"
#include "Zenith/VM/State.h"

namespace fs = std::filesystem;

namespace {

    enum class Mode { None, Ast, Flat, Vm };

    void print_usage(const char* prog_name)
    {
        std::cout
            << "Usage: " << prog_name << " [file.zen] <mode>\n"
            << "  --ast         Dump S-expression AST to stdout\n"
            << "  --flat        Dump flat AST table to stdout\n"
            << "  --vm          Run VM verification test\n"
            << "  -h, --help    Show this help message\n";
    }

    bool read_file(const fs::path& path, std::string& out_content)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file) return false;
        out_content.assign(std::istreambuf_iterator<char>(file),
                           std::istreambuf_iterator<char>());
        return true;
    }

}

int main(int argc, char* argv[])
{
    if (argc < 2) { print_usage(argv[0]); return 1; }

    std::string file_path;
    Mode mode = Mode::None;

    for (int i = 1; i < argc; ++i)
    {
        std::string_view arg = argv[i];
        if (arg == "-h" || arg == "--help") { print_usage(argv[0]); return 0; }
        else if (arg == "--flat") mode = Mode::Flat;
        else if (arg == "--ast")  mode = Mode::Ast;
        else if (arg == "--vm")   mode = Mode::Vm;
        else if (!arg.empty() && arg[0] == '-') {
            std::cerr << "Error: Unknown option '" << arg << "'\n";
            print_usage(argv[0]);
            return 1;
        }
        else file_path = std::string(arg);
    }

    if (mode == Mode::None) {
        std::cerr << "Error: No execution mode specified (--ast, --flat, --vm).\n";
        print_usage(argv[0]);
        return 1;
    }

    if (mode == Mode::Vm)
    {
        using namespace Zenith;

        std::string_view callee_src = R"(
            .params 2
            .regs 6

            Neg.I32   R2, R1
            Sub.I32   R3, R0, R1
            DivS.I32  R4, R0, R2
            RemS.I32  R5, R0, R2
            Lt.I32    R2, R4, R5
            Jz        R2, dead_path
            Jmp       live_path

            dead_path:
            Return    R4, 1

            live_path:
            Add.I32   R0, R3, R5
            Return    R0, 1
        )";

        std::string_view main_src = R"(
            .params 0
            .regs 10

            .const F32 50.0
            .const F32 2.0

            LoadConst R0, K0
            LoadConst R1, K1
            Div.F32   R2, R0, R1

            LoadImm   R3, 100
            Mov       R4, R3
            LoadImm   R5, 30
            DivU.U32  R6, R4, R5
            RemU.U32  R7, R4, R5
            Mul.I32   R8, R6, R7
            Eq.I32    R9, R8, R5
            LNot      R9, R9
            Jnz       R9, fail

            LoadImm   R6, 20
            LoadImm   R7, 6
            Call      R6, 0

            LoadImm   R7, 16
            Eq.I32    R8, R6, R7
            Jz        R8, fail
            Halt

            fail:
            LoadImm   R0, -1
            Halt
        )";

        std::string err;
        Module module;

        // ── Assemble callee → proto 0
        ProtoBuilder callee_builder;
        if (!Assembler::assemble(callee_builder, callee_src, &err)) {
            std::cerr << "Assembler error (callee): " << err << "\n";
            return 1;
        }
        uint16_t callee_id = module.add(std::move(callee_builder));

        // ── Assemble main → proto 1
        ProtoBuilder main_builder;
        if (!Assembler::assemble(main_builder, main_src, &err)) {
            std::cerr << "Assembler error (main): " << err << "\n";
            return 1;
        }
        uint16_t main_id = module.add(std::move(main_builder));

        // ── Freeze
        module.freeze();

        // ── Dump
        std::cout << "; --- Callee Proto (#" << callee_id << ") ---\n";
        Disassembler::dump(module, callee_id, std::cout);
        std::cout << "\n; --- Main Proto (#" << main_id << ") ---\n";
        Disassembler::dump(module, main_id, std::cout);

        // ── Execute
        auto shared_module = std::make_shared<const Module>(std::move(module));
        State state;
        state.set_module(shared_module);
        state.prepare(main_id);
        Executor::execute(state);

        bool pass = (state.reg(6).i32 == 16)
                 && (state.reg(2).f32 == 25.0f)
                 && (state.reg(0).f32 == 50.0f);

        std::cout << "\nVM test: " << (pass ? "PASSED" : "FAILED")
                  << " (callee_ret=" << state.reg(6).i32
                  << ", div_f32=" << state.reg(2).f32 << ")\n";
        std::cout.flush();
        return pass ? 0 : 1;
    }

    // ── Parser path
    if (file_path.empty()) {
        std::cerr << "Error: No input file specified.\n";
        print_usage(argv[0]);
        return 1;
    }

    std::string source;
    if (!read_file(file_path, source)) {
        std::cerr << "Error: Could not open file '" << file_path << "'\n";
        return 1;
    }

    Zenith::Parser parser(source);
    uint32_t root = parser.parse_program();

    if (parser.has_errors()) {
        parser.dump_errors(std::cerr);
        return 1;
    }

    if (mode == Mode::Flat) parser.dump_flat_ast(std::cout);
    else if (mode == Mode::Ast) parser.dump_tree_ast(std::cout, root);
    std::cout.flush();
    return 0;
}