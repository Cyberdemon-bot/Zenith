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

    enum class Mode { Ast, TypedAst, FlatTypes };

    void PrintUsage(const char* progName)
    {
        std::cout
            << "Usage: " << progName << " <file.zen> [options]\n"
            << "  --ast         Dump S-expression AST to stdout (default)\n"
            << "  --types       Run TypeChecker and dump typed S-expression AST\n"
            << "  --flat-types  Run TypeChecker and dump flat node types table\n"
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

}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        PrintUsage(argv[0]);
        return 1;
    }

    std::string filePath;
    Mode mode = Mode::Ast;

    for (int i = 1; i < argc; ++i)
    {
        std::string_view arg = argv[i];
        if (arg == "-h" || arg == "--help")
        {
            PrintUsage(argv[0]);
            return 0;
        }
        else if (arg == "--ast")        mode = Mode::Ast;
        else if (arg == "--types")      mode = Mode::TypedAst;
        else if (arg == "--flat-types") mode = Mode::FlatTypes;
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

    Zenith::Parser parser;
    auto ast = parser.ParseProgram(source);

    if (parser.HasErrors())
    {
        std::cerr << Zenith::Dumper::DumpDiagnostics(ast, parser.Diagnostics(), "Parser");
        return 1;
    }

    if (mode == Mode::Ast)
    {
        std::cout << Zenith::Dumper::DumpTreeAst(ast);
        std::cout.flush();
        return 0;
    }

    Zenith::TypeChecker checker;
    auto typed = checker.Validate(ast);
    if (checker.HasErrors())
    {
        std::cerr << Zenith::Dumper::DumpDiagnostics(ast, checker.Diagnostics(), "TypeChecker");
        return 1;
    }

    if (mode == Mode::FlatTypes)
        std::cout << Zenith::Dumper::DumpTypes(typed.ast, typed.node_types);
    else
        std::cout << Zenith::Dumper::DumpTypedAst(typed.ast, typed.node_types);

    std::cout.flush();
    return 0;
}
