#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>

#include "Zenith/Core/ProfileTimer.h"
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
        Vm,
        VmBench,
        FibBench
    };

    void print_usage(const char* prog_name)
    {
        std::cout
            << "Usage: " << prog_name << " [file.zen] <mode>\n"
            << "Options:\n"
            << "  --ast         Dump S-expression AST to stdout\n"
            << "  --flat        Dump flat AST table to stdout\n"
            << "  --vm          Run hardcoded VM test proto (Collatz)\n"
            << "  --vmbench     Benchmark: prime counting, VM vs native C\n"
            << "  --fibbench    Benchmark: iterative Fibonacci, VM vs native C\n"
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
        else if (arg == "--flat")     mode = Mode::Flat;
        else if (arg == "--ast")      mode = Mode::Ast;
        else if (arg == "--vm")       mode = Mode::Vm;
        else if (arg == "--vmbench")  mode = Mode::VmBench;
        else if (arg == "--fibbench") mode = Mode::FibBench;
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
        std::cerr << "Error: No execution mode specified "
                     "(--ast, --flat, --vm, --vmbench, --fibbench).\n";
        print_usage(argv[0]);
        return 1;
    }

    // ================================================================
    //  --vmbench : prime counting (compute-heavy, div/mod, branches)
    // ================================================================
    if (mode == Mode::VmBench)
    {
        using namespace Zenith;

        constexpr int16_t LIMIT = 30000;
        constexpr int RUNS = 3;

        // ---------- Native C ----------
        volatile uint32_t c_count = 0;
        float c_best = 1e9f;

        for (int run = 0; run < RUNS; ++run)
        {
            float t;
            {
                ZN_PROFILE_SCOPE(&t);
                uint32_t count = 0;
                for (int32_t n = 2; n <= LIMIT; ++n)
                {
                    bool prime = true;
                    for (int32_t d = 2; d * d <= n; ++d)
                    {
                        if (n % d == 0) { prime = false; break; }
                    }
                    if (prime) ++count;
                }
                c_count = count;
            }
            if (t < c_best) c_best = t;
        }

        // ---------- Bytecode VM ----------
        //  r0 = n, r1 = count, r2 = d, r3 = scratch, r4 = scratch,
        //  r5 = 0, r6 = 1, r7 = LIMIT
        Proto proto(0, 8);

        proto.commit_instruction(Instruction::make<Format::iAsBx>(Opcode::LoadImm, Type::None, 0, 2));
        proto.commit_instruction(Instruction::make<Format::iAsBx>(Opcode::LoadImm, Type::None, 1, 0));
        proto.commit_instruction(Instruction::make<Format::iAsBx>(Opcode::LoadImm, Type::None, 5, 0));
        proto.commit_instruction(Instruction::make<Format::iAsBx>(Opcode::LoadImm, Type::None, 6, 1));
        proto.commit_instruction(Instruction::make<Format::iAsBx>(Opcode::LoadImm, Type::None, 7, LIMIT));

        uint32_t outer = proto.commit_instruction(
            Instruction::make<Format::iABC>(Opcode::Lt, Type::I32, 3, 7, 0)
        );
        uint32_t exit_jmp = proto.commit_instruction(
            Instruction::make<Format::iAsBx>(Opcode::Jnz, 3, 0)
        );
        proto.commit_instruction(
            Instruction::make<Format::iAsBx>(Opcode::LoadImm, Type::None, 2, 2)
        );

        uint32_t inner = proto.commit_instruction(
            Instruction::make<Format::iABC>(Opcode::Mul, Type::U32, 4, 2, 2)
        );
        proto.commit_instruction(
            Instruction::make<Format::iABC>(Opcode::Lt, Type::I32, 3, 0, 4)
        );
        uint32_t prime_jmp = proto.commit_instruction(
            Instruction::make<Format::iAsBx>(Opcode::Jnz, 3, 0)
        );
        proto.commit_instruction(
            Instruction::make<Format::iABC>(Opcode::RemS, Type::I32, 4, 0, 2)
        );
        proto.commit_instruction(
            Instruction::make<Format::iABC>(Opcode::Eq, Type::U32, 3, 4, 5)
        );
        uint32_t not_prime_jmp = proto.commit_instruction(
            Instruction::make<Format::iAsBx>(Opcode::Jnz, 3, 0)
        );
        proto.commit_instruction(
            Instruction::make<Format::iABC>(Opcode::Add, Type::U32, 2, 2, 6)
        );
        uint32_t inner_back = proto.commit_instruction(
            Instruction::make<Format::iAsBx>(Opcode::Jmp, 0, 0)
        );

        uint32_t prime = proto.commit_instruction(
            Instruction::make<Format::iABC>(Opcode::Add, Type::U32, 1, 1, 6)
        );

        uint32_t not_prime = proto.commit_instruction(
            Instruction::make<Format::iABC>(Opcode::Add, Type::U32, 0, 0, 6)
        );
        uint32_t outer_back = proto.commit_instruction(
            Instruction::make<Format::iAsBx>(Opcode::Jmp, 0, 0)
        );

        uint32_t done = proto.commit_instruction(
            Instruction::make<Format::iNone>(Opcode::Halt)
        );

        proto.patch_jmp(exit_jmp,      int16_t(int32_t(done)      - int32_t(exit_jmp      + 1)));
        proto.patch_jmp(prime_jmp,     int16_t(int32_t(prime)     - int32_t(prime_jmp     + 1)));
        proto.patch_jmp(not_prime_jmp, int16_t(int32_t(not_prime) - int32_t(not_prime_jmp + 1)));
        proto.patch_jmp(inner_back,    int16_t(int32_t(inner)     - int32_t(inner_back    + 1)));
        proto.patch_jmp(outer_back,    int16_t(int32_t(outer)     - int32_t(outer_back    + 1)));

        State state;
        uint16_t id = state.add_proto(std::move(proto));

        state.prepare(id);
        Executor::execute(state);

        float vm_best = 1e9f;
        uint32_t vm_count = 0;
        for (int run = 0; run < RUNS; ++run)
        {
            state.prepare(id);
            float t;
            {
                ZN_PROFILE_SCOPE(&t);
                Executor::execute(state);
            }
            if (t < vm_best) vm_best = t;
            vm_count = state.reg(1).u32;
        }

        const bool  match = (c_count == vm_count);
        const float ratio = vm_best / c_best;

        std::cout << "Prime counting up to " << LIMIT
                  << "  (best of " << RUNS << " runs)\n"
                  << "  native C (-O3):  " << c_count  << " primes, " << c_best << " ms\n"
                  << "  bytecode VM:     " << vm_count << " primes, " << vm_best << " ms\n"
                  << "  ratio:           " << ratio    << "x\n"
                  << "  match:           " << (match ? "yes" : "NO — BUG!") << "\n";
        std::cout.flush();
        return match ? 0 : 2;
    }

    // ================================================================
    //  --fibbench : iterative fibonacci (loop-only, no div/mod)
    // ================================================================
    if (mode == Mode::FibBench)
    {
        using namespace Zenith;

        // fib(LIMIT) với uint32_t wrapping. LIMIT ≤ 32767 vì LoadImm dùng sBx.
        // LIMIT = 30000 → fib(30000) mod 2^32.
        constexpr int16_t LIMIT = 30000;
        constexpr int RUNS = 3;

        // ---------- Native C ----------
        volatile uint32_t c_fib = 0;
        float c_best = 1e9f;

        for (int run = 0; run < RUNS; ++run)
        {
            float t;
            {
                ZN_PROFILE_SCOPE(&t);
                uint32_t a = 0, b = 1;
                for (int32_t i = 0; i < LIMIT; ++i)
                {
                    uint32_t tmp = a + b;
                    a = b;
                    b = tmp;
                }
                c_fib = a;
            }
            if (t < c_best) c_best = t;
        }

        // ---------- Bytecode VM ----------
        //  r0 = n (LIMIT)
        //  r1 = i (counter)
        //  r2 = a
        //  r3 = b
        //  r4 = tmp
        //  r5 = const 1
        //  r6 = scratch
        Proto proto(0, 8);

        proto.commit_instruction(Instruction::make<Format::iAsBx>(Opcode::LoadImm, Type::None, 0, LIMIT));
        proto.commit_instruction(Instruction::make<Format::iAsBx>(Opcode::LoadImm, Type::None, 1, 0));
        proto.commit_instruction(Instruction::make<Format::iAsBx>(Opcode::LoadImm, Type::None, 2, 0));
        proto.commit_instruction(Instruction::make<Format::iAsBx>(Opcode::LoadImm, Type::None, 3, 1));
        proto.commit_instruction(Instruction::make<Format::iAsBx>(Opcode::LoadImm, Type::None, 5, 1));

        // loop: pc=5
        uint32_t loop = proto.commit_instruction(
            Instruction::make<Format::iABC>(Opcode::Lt, Type::I32, 6, 1, 0)      // r6 = (i < n)
        );
        uint32_t exit_jmp = proto.commit_instruction(
            Instruction::make<Format::iAsBx>(Opcode::Jz, 6, 0)                   // if !(i<n) → done
        );
        proto.commit_instruction(
            Instruction::make<Format::iABC>(Opcode::Add, Type::U32, 4, 2, 3)     // tmp = a + b
        );
        proto.commit_instruction(
            Instruction::make<Format::iABC>(Opcode::Mov, Type::None, 2, 3, 0)     // a = b
        );
        proto.commit_instruction(
            Instruction::make<Format::iABC>(Opcode::Mov, Type::None, 3, 4, 0)     // b = tmp
        );
        proto.commit_instruction(
            Instruction::make<Format::iABC>(Opcode::Add, Type::U32, 1, 1, 5)     // i++
        );
        uint32_t back = proto.commit_instruction(
            Instruction::make<Format::iAsBx>(Opcode::Jmp, 0, 0)                  // goto loop
        );
        uint32_t done = proto.commit_instruction(
            Instruction::make<Format::iNone>(Opcode::Halt)
        );

        proto.patch_jmp(exit_jmp, int16_t(int32_t(done) - int32_t(exit_jmp + 1)));
        proto.patch_jmp(back,     int16_t(int32_t(loop) - int32_t(back     + 1)));

        State state;
        uint16_t id = state.add_proto(std::move(proto));

        state.prepare(id);
        Executor::execute(state);

        float vm_best = 1e9f;
        uint32_t vm_fib = 0;
        for (int run = 0; run < RUNS; ++run)
        {
            state.prepare(id);
            float t;
            {
                ZN_PROFILE_SCOPE(&t);
                Executor::execute(state);
            }
            if (t < vm_best) vm_best = t;
            vm_fib = state.reg(2).u32;
        }

        const bool  match = (c_fib == vm_fib);
        const float ratio = vm_best / c_best;

        std::cout << "Iterative Fibonacci, n = " << LIMIT
                  << "  (best of " << RUNS << " runs)\n"
                  << "  native C:        " << c_fib  << ", " << c_best << " ms\n"
                  << "  bytecode VM:     " << vm_fib << ", " << vm_best << " ms\n"
                  << "  ratio:           " << ratio  << "x\n"
                  << "  match:           " << (match ? "yes" : "NO — BUG!") << "\n";
        std::cout.flush();
        return match ? 0 : 2;
    }

    // ================================================================
    //  --vm : Collatz(27)
    // ================================================================
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

        float exeTime;
        {
            ZN_PROFILE_SCOPE(&exeTime);
            Executor::execute(state);
        }

        std::cout << "Collatz(27): " << state.reg(1).i32
                  << " steps (n=" << state.reg(0).i32 << ") execute time: " << exeTime << "\n";
        std::cout.flush();
        return 0;
    }

    // ================================================================
    //  --ast / --flat : parser
    // ================================================================
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