#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>

#include "Zenith/Compiler/Parser.h"

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

} // namespace

int main(int argc, char* argv[])
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
