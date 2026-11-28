#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>

#include "Zenith/Compiler/CodeGen.h"
#include "Zenith/Compiler/Dumper.h"
#include "Zenith/Compiler/Parser.h"
#include "Zenith/Compiler/SignatureTable.h"
#include "Zenith/Compiler/TypeChecker.h"
#include "Zenith/VM/Executor.h"
#include "Zenith/VM/Proto.h"
#include "Zenith/VM/State.h"

namespace fs = std::filesystem;

namespace {

    constexpr std::string_view DEMO_SOURCE = R"zen(
func fib(n: i64) -> i64 {
    if n <= 1 {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}

func main() -> i64 {
    let f = fib(10);
    let mut sum: i64 = 0;
    let bonus = 'outer: for {
        let mut i: i64 = 0;
        for i < 5 {
            i = i + 1;
            sum = sum + i;
            if sum >= 20 {
                break 'outer (sum as i32) as i64;
            }
        }
    };
    return f + bonus;
}
)zen";

    void PrintUsage(const char* progName)
    {
        std::cout
            << "Usage: " << progName << " [file.zen | --demo-source]\n"
            << "  --demo-source  Run built-in demo source (default when no args)\n"
            << "  -h, --help     Show this help message\n";
    }

    bool ReadFile(const fs::path& path, std::string& outContent)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file) return false;
        outContent.assign(std::istreambuf_iterator<char>(file),
                           std::istreambuf_iterator<char>());
        return true;
    }

    void PrintResult(const Zenith::State& state, Zenith::CTType ret_type)
    {
        using Zenith::CTType;
        using Zenith::Type;
        const Zenith::Value& v = state.Reg(0);
        switch (ret_type.kind)
        {
            case CTType::Kind::Unit: std::cout << "()\n"; break;
            case CTType::Kind::Bool: std::cout << (v.u64 != 0 ? "true" : "false") << '\n'; break;
            case CTType::Kind::I8:
            case CTType::Kind::I16:
            case CTType::Kind::I32:  std::cout << v.GetAs<Type::I32>() << '\n'; break;
            case CTType::Kind::U8:
            case CTType::Kind::U16:
            case CTType::Kind::U32:  std::cout << v.GetAs<Type::U32>() << '\n'; break;
            case CTType::Kind::I64:  std::cout << v.GetAs<Type::I64>() << '\n'; break;
            case CTType::Kind::U64:  std::cout << v.GetAs<Type::U64>() << '\n'; break;
            case CTType::Kind::F32:  std::cout << v.GetAs<Type::F32>() << '\n'; break;
            case CTType::Kind::F64:  std::cout << v.GetAs<Type::F64>() << '\n'; break;
            default:                 std::cout << v.u64 << '\n'; break;
        }
    }

    int RunSource(std::string_view source)
    {
        Zenith::Parser parser;
        auto ast = parser.ParseProgram(source);
        if (parser.HasErrors())
        {
            std::cerr << Zenith::Dumper::DumpDiagnostics(ast, parser.Diagnostics(), "Parser");
            return 1;
        }

        Zenith::TypeChecker checker;
        auto typed = checker.Validate(ast);
        if (checker.HasErrors())
        {
            std::cerr << Zenith::Dumper::DumpDiagnostics(ast, checker.Diagnostics(), "TypeChecker");
            return 1;
        }

        auto mod = Zenith::CreateRef<Zenith::Module>();
        Zenith::CodeGen codegen;
        if (!codegen.Generate(typed, *mod))
        {
            std::cerr << Zenith::Dumper::DumpDiagnostics(ast, codegen.Diagnostics(), "CodeGen");
            return 1;
        }

        if (!mod->HasEntry())
        {
            std::cerr << "Error: No 'main' entry function found.\n";
            return 1;
        }

        Zenith::State state;
        state.SetModule(mod);
        state.Prepare(mod->GetEntry());

        if (!Zenith::Executor::Execute(state))
        {
            const auto& err = state.GetError();
            std::cerr << "RuntimeError: " << Zenith::RuntimeErrorToString(err.code)
                      << " (Proto #" << err.proto_id << ", PC " << err.pc << ")\n";
            return 1;
        }

        auto main_sig = typed.signatures->Find("main");
        PrintResult(state, main_sig ? main_sig->ret_type : Zenith::CTType::Unit());
        return 0;
    }

}

int main(int argc, char* argv[])
{
    if (argc < 2)
        return RunSource(DEMO_SOURCE);

    std::string filePath;
    bool useDemo = false;

    for (int i = 1; i < argc; ++i)
    {
        std::string_view arg = argv[i];
        if (arg == "-h" || arg == "--help")
        {
            PrintUsage(argv[0]);
            return 0;
        }
        else if (arg == "--demo-source")
        {
            useDemo = true;
        }
        else if (!arg.empty() && arg[0] == '-')
        {
            std::cerr << "Error: Unknown option '" << arg << "'\n";
            PrintUsage(argv[0]);
            return 1;
        }
        else
        {
            filePath = std::string(arg);
        }
    }

    if (useDemo)
        return RunSource(DEMO_SOURCE);

    if (filePath.empty())
    {
        std::cerr << "Error: No input file specified.\n";
        PrintUsage(argv[0]);
        return 1;
    }

    std::string source;
    if (!ReadFile(filePath, source))
    {
        std::cerr << "Error: Could not open file '" << filePath << "'\n";
        return 1;
    }

    return RunSource(source);
}
