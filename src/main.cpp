#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>

#include "Zenith/Compiler/Parser.h"
#include "Zenith/Compiler/TypeChecker.h"

namespace fs = std::filesystem;

namespace {

    enum class Mode { None, Ast, Flat };

    void PrintUsage(const char* progName)
    {
        std::cout
            << "Usage: " << progName << " <file.zen> [options]\n"
            << "  --ast         Dump S-expression AST to stdout (default)\n"
            << "  --flat        Dump flat AST table to stdout\n"
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
        Mode mode = Mode::None;

        for (int i = 1; i < argc; ++i)
        {
            std::string_view arg = argv[i];
            if (arg == "-h" || arg == "--help")
            {
                PrintUsage(argv[0]);
                return 0;
            }
            else if (arg == "--flat") mode = Mode::Flat;
            else if (arg == "--ast")  mode = Mode::Ast;
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

        if (mode == Mode::None)
            mode = Mode::Ast;

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
            parser.DumpErrors(std::cerr);
            return 1;
        }

        if (mode == Mode::Flat)
            parser.DumpFlatAst(std::cout);
        else
            parser.DumpTreeAst(std::cout, ast.root);

        std::cout.flush();
        return 0;
    }

    void RunCheckerTest(std::string_view label, std::string_view source)
    {
        std::cout << "=== " << label << " ===\n";
        Zenith::Parser parser(source);
        auto ast = parser.ParseProgram();
        parser.DumpErrors(std::cout);

        if (parser.HasErrors())
        {
            std::cout << "\n";
            return;
        }

        Zenith::TypeChecker checker(ast);
        bool ok = checker.Validate();
        std::cout << "[TypeChecker] Result: " << (ok ? "PASS" : "FAIL") << "\n";
        checker.DumpErrors(std::cout);
        std::cout << "\n";
    }

}

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    // Test 1: Valid (adapted from parser_let_and_literals + parser_if_expr)
    constexpr std::string_view kValidSource = R"(
func helper(x: i64) -> i64 {
    return if x > 0 => 1 else => -1;
}

func main() {
    let a = 42;
    let b = 3.14;
    let t = true;
    let typed: i64 = 1;
    let mut counter = 0;
    counter = counter + helper(a);
    let nested = if (if a > 0 => true else => false) => 1 else => -1;
}
)";

    // Test 2: Valid syntax, intentional type errors
    constexpr std::string_view kInvalidSource = R"(
func main() {
    let typed: i64 = true;
    let mut counter = 0;
    counter = 3.14;
    let bad_if = if 123 => 1 else => 2.5;
}
)";

    RunCheckerTest("Test 1: Valid Program", kValidSource);
    RunCheckerTest("Test 2: Type Mismatch Program", kInvalidSource);

    return 0;
}
