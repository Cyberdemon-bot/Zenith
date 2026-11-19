#include "TypeCheckerStep.h"
#include "Diff.h"
#include "Zenith/Compiler/Diagnostic.h"
#include "Zenith/Compiler/Dumper.h"
#include "Zenith/Compiler/Parser.h"
#include "Zenith/Compiler/TypeChecker.h"
#include "Zenith/Core/ProfileTimer.h"

namespace {

    void AppendTokenLocation(Zenith::AstView ast, uint32_t token_idx, uint32_t& line, uint32_t& col)
    {
        if (ast.tokens.empty())
        {
            line = 0;
            col = 0;
            return;
        }
        if (token_idx >= ast.tokens.size())
            token_idx = static_cast<uint32_t>(ast.tokens.size()) - 1;
        line = ast.tokens[token_idx].line;
        col = ast.tokens[token_idx].column;
    }

}

TypeCheckerStep::Result TypeCheckerStep::Run(std::string_view source)
{
    Result res;
    ZN_PROFILE_SCOPE(&res.profile.totalTimeMs);

    Zenith::Parser parser(source);
    Zenith::AstView ast;
    {
        ZN_PROFILE_SCOPE(&res.profile.parseTimeMs);
        ast = parser.ParseProgram();
        if (parser.HasErrors())
        {
            for (const auto& err : parser.Errors())
            {
                uint32_t line = 0, col = 0;
                AppendTokenLocation(ast, err.token, line, col);
                res.errors.push_back({ line, col, err.message });
            }
            return res;
        }
    }

    Zenith::TypeChecker checker(ast);
    {
        ZN_PROFILE_SCOPE(&res.profile.checkTimeMs);
        checker.Validate();
        for (const auto& diag : checker.Diagnostics())
        {
            uint32_t line = 0, col = 0;
            AppendTokenLocation(ast, diag.token, line, col);
            res.errors.push_back({ line, col, Zenith::FormatDiagnostic(diag) });
        }
    }

    {
        ZN_PROFILE_SCOPE(&res.profile.dumpTimeMs);
        res.typedAst = Diff::Trim(Diff::NormalizeNewlines(
            Zenith::Dumper::DumpTypedAst(ast, checker.NodeTypes())));
    }

    return res;
}
