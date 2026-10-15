#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include "Zenith/Compiler/Parser.h"

namespace fs = std::filesystem;

namespace {

    void print_usage(const char* prog_name)
    {
        std::cout
            << "Usage: " << prog_name << " <file.zen> [options]\n"
            << "Options:\n"
            << "  --ast         Dump S-expression AST to stdout (default)\n"
            << "  --flat        Dump flat AST table to stdout\n"
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
    bool dump_flat = false;

    for (int i = 1; i < argc; ++i)
    {
        std::string_view arg = argv[i];
        if (arg == "-h" || arg == "--help")
        {
            print_usage(argv[0]);
            return 0;
        }
        else if (arg == "--flat")
        {
            dump_flat = true;
        }
        else if (arg == "--ast")
        {
            dump_flat = false;
        }
        else if (!arg.empty() && arg[0] == '-')
        {
            std::cerr << "Error: Unknown option '" << arg << "'\n";
            print_usage(argv[0]);
            return 1;
        }
        else
        {
            file_path = std::string(arg);
        }
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

    if (dump_flat)
    {
        parser.dump_flat_ast(std::cout);
    }
    else
    {
        parser.dump_tree_ast(std::cout, root);
    }

    std::cout.flush();
    return 0;
}
