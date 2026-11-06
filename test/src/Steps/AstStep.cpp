#include "AstStep.h"
#include "Diff.h"
#include "Zenith/Compiler/Parser.h"
#include "Zenith/Core/ProfileTimer.h"

#include <cstdio>
#include <sstream>

std::vector<ErrorInfo> AstStep::CollectErrors(const Zenith::Parser& parser)
{
    std::ostringstream out;
    parser.DumpErrors(out);

    std::vector<ErrorInfo> errors;
    for (const auto& line : Diff::SplitLines(Diff::NormalizeNewlines(out.str())))
    {
        if (line.rfind("[Error #", 0) == 0)
        {
            auto close = line.find("] ");
            errors.push_back({ 0, 0, close == std::string::npos ? "" : line.substr(close + 2) });
        }
        else if (!errors.empty() && errors.back().line == 0)
        {
            auto pos = line.find("--> Line ");
            unsigned l = 0, c = 0;
            if (pos != std::string::npos && std::sscanf(line.c_str() + pos + 9, "%u:%u", &l, &c) == 2)
            {
                errors.back().line = l;
                errors.back().col = c;
            }
        }
    }
    return errors;
}

AstStep::Result AstStep::Run(std::string_view source)
{
    Result res;
    ZN_PROFILE_SCOPE(&res.profile.totalTimeMs);

    Zenith::Parser parser(source);
    uint32_t root = 0;
    {
        ZN_PROFILE_SCOPE(&res.profile.parseTimeMs);
        auto ast = parser.ParseProgram();
        root = ast.root;
        res.errors = CollectErrors(parser);
    }

    {
        ZN_PROFILE_SCOPE(&res.profile.dumpTimeMs);
        std::ostringstream tree;
        parser.DumpTreeAst(tree, root);
        res.astTree = Diff::Trim(Diff::NormalizeNewlines(tree.str()));
    }

    return res;
}
