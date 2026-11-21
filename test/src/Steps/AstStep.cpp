#include "AstStep.h"
#include "Diff.h"
#include "Zenith/Compiler/Diagnostic.h"
#include "Zenith/Compiler/Dumper.h"
#include "Zenith/Compiler/Parser.h"
#include "Zenith/Core/ProfileTimer.h"

AstStep::Result AstStep::Run(std::string_view source)
{
    Result res;
    ZN_PROFILE_SCOPE(&res.profile.totalTimeMs);

    Zenith::Parser parser(source);
    Zenith::AstView ast;
    {
        ZN_PROFILE_SCOPE(&res.profile.parseTimeMs);
        ast = parser.ParseProgram();
        for (const auto& diag : parser.Diagnostics())
        {
            uint32_t tok_idx = diag.token;
            if (!ast.tokens.empty() && tok_idx >= ast.tokens.size())
                tok_idx = static_cast<uint32_t>(ast.tokens.size()) - 1;

            uint32_t line = 0;
            uint32_t col = 0;
            if (!ast.tokens.empty())
            {
                line = ast.tokens[tok_idx].line;
                col = ast.tokens[tok_idx].column;
            }
            res.errors.push_back({ line, col, Zenith::FormatDiagnostic(diag) });
        }
    }

    {
        ZN_PROFILE_SCOPE(&res.profile.dumpTimeMs);
        res.astTree = Diff::Trim(Diff::NormalizeNewlines(Zenith::Dumper::DumpTreeAst(ast)));
    }

    return res;
}
