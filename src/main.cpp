#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>

#include "Zenith/Compiler/Dumper.h"
#include "Zenith/Compiler/Parser.h"
#include "Zenith/Compiler/TypeChecker.h"

namespace fs = std::filesystem;

namespace {

    void PrintUsage(const char* progName)
    {
        std::cout
            << "Usage: " << progName << " <file.zen> [options]\n"
            << "  --ast         Dump S-expression AST to stdout (default)\n"
            << "  --types       Run TypeChecker and dump inferred types\n"
            << "  -h, --help    Show this help message\n";
    }

    bool ReadFile(const fs::path& path, std::string& outContent)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file) return false;
        outContent.assign(std::istreambuf_iterator<char>(file),
                           std::istreambuf_iterator<char>());
        return true;
    }

    [[maybe_unused]] int RunCli(int argc, char* argv[])
    {
        if (argc < 2)
        {
            PrintUsage(argv[0]);
            return 1;
        }

        std::string filePath;
        bool checkTypes = false;

        for (int i = 1; i < argc; ++i)
        {
            std::string_view arg = argv[i];
            if (arg == "-h" || arg == "--help")
            {
                PrintUsage(argv[0]);
                return 0;
            }
            else if (arg == "--types") checkTypes = true;
            else if (arg == "--ast")   checkTypes = false;
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

        Zenith::Parser parser(source);
        auto ast = parser.ParseProgram();

        if (parser.HasErrors())
        {
            std::cerr << Zenith::Dumper::DumpParseErrors(ast, parser.Errors());
            return 1;
        }

        if (checkTypes)
        {
            Zenith::TypeChecker checker(ast);
            bool ok = checker.Validate();
            if (!ok)
            {
                std::cerr << Zenith::Dumper::DumpTypeErrors(ast, checker.Diagnostics());
                return 1;
            }
            std::cout << Zenith::Dumper::DumpTypedAst(ast, checker.NodeTypes());
        }
        else
        {
            std::cout << Zenith::Dumper::DumpTreeAst(ast);
        }

        std::cout.flush();
        return 0;
    }

    void RunCheckerTest(std::string_view label, std::string_view source)
    {
        std::cout << "=== " << label << " ===\n";
        Zenith::Parser parser(source);
        auto ast = parser.ParseProgram();
        std::cout << Zenith::Dumper::DumpParseErrors(ast, parser.Errors());

        if (parser.HasErrors())
        {
            std::cout << "\n";
            return;
        }

        Zenith::TypeChecker checker(ast);
        bool ok = checker.Validate();
        std::cout << "[TypeChecker] Result: " << (ok ? "PASS" : "FAIL") << "\n";
        std::cout << Zenith::Dumper::DumpTypeErrors(ast, checker.Diagnostics());
        if (ok) std::cout << Zenith::Dumper::DumpTypes(ast, checker.NodeTypes());
        std::cout << "\n";
    }

}

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    // Demo 1: Valid scalar types, backward literal inference, casts, and control flow
    constexpr std::string_view kValidSource = R"(
func add_u8(x: u8, y: u8) -> u8 {
    return x + y;
}

func main() {
    let a = 1;
    let b: u8 = a + 2;
    let c: f32 = 1.5;
    let d = (b as i64) + (c as i64);

    let mut sum = 0;
    let res: i64 = 'outer: for {
        sum = sum + 1;
        if sum < 3 {
            continue 'outer;
        }
        break 'outer sum;
    };

    let blk = 'block_val: {
        if d > 0 {
            break 'block_val 10;
        }
        break 'block_val 20;
    };
}
)";

    // Demo 2: Numeric range, cast, unary '-', and type mismatch errors
    constexpr std::string_view kTypeErrorsSource = R"(
func main() {
    let overflow: u8 = 256;
    let neg_unsigned: u8 = 5;
    let bad_neg = -neg_unsigned;
    let bad_cast = 3.14 as bool;
    let bad_if = if 123 => 1 else => 2.5;
}
)";

    // Demo 3: Scope, mutability, and control-flow errors
    constexpr std::string_view kControlErrorsSource = R"(
func need_ret() -> i64 {
    let x = 1;
}

func main() {
    let x = 10;
    let x = 20;
    x = 30;

    let missing: i64 = 'blk: {
        let a = 1;
    };

    'loop_lbl: {
        continue 'loop_lbl;
    };

    break 'ghost;
}
)";

    RunCheckerTest("Demo 1: Valid Program (Inference & Control Flow)", kValidSource);
    RunCheckerTest("Demo 2: Numeric, Cast & Type Errors", kTypeErrorsSource);
    RunCheckerTest("Demo 3: Scope, Mutability & Control Flow Errors", kControlErrorsSource);

    return 0;
}
