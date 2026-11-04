#include "Tester.h"
#include "Diff.h"

#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char* argv[])
{
    Options opt;
    if (std::getenv("NO_COLOR")) Diff::s_Color = false;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-u" || arg == "--update") opt.update = true;
        else if (arg == "-d" || arg == "--dump") opt.dump = true;
        else if (arg == "--no-color") Diff::s_Color = false;
        else if (arg == "-h" || arg == "--help") { Tester::PrintUsage(); return 0; }
        else if ((arg == "-f" || arg == "--filter") && i + 1 < argc) opt.filter = argv[++i];
        else if (!arg.empty() && arg[0] == '-')
        {
            std::cerr << "Unknown or incomplete option: " << arg << "\n";
            Tester::PrintUsage();
            return 2;
        }
        else opt.dir = arg;
    }

    return Tester::Run(opt);
}
