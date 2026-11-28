#pragma once

#include "Diff.h"
#include "TestCase.h"

#include "Zenith/Compiler/Ast.h"
#include "Zenith/Compiler/Diagnostic.h"
#include "Zenith/VM/Disassembler.h"
#include "Zenith/VM/Proto.h"

#include <span>
#include <sstream>
#include <string>
#include <vector>

namespace StepUtils {

    inline std::vector<ErrorInfo> CollectDiagnostics(
        Zenith::AstView ast,
        std::span<const Zenith::Diagnostic> diagnostics)
    {
        std::vector<ErrorInfo> out;
        out.reserve(diagnostics.size());
        for (const auto& diag : diagnostics)
        {
            uint32_t line = 0, col = 0;
            if (!ast.tokens.empty())
            {
                uint32_t tok = diag.token;
                if (tok >= ast.tokens.size())
                    tok = static_cast<uint32_t>(ast.tokens.size()) - 1;
                line = ast.tokens[tok].line;
                col = ast.tokens[tok].column;
            }
            out.push_back({ line, col, Zenith::FormatDiagnostic(diag) });
        }
        return out;
    }

    inline std::string DumpDisasm(const Zenith::Module& mod)
    {
        std::ostringstream os;
        for (uint16_t id = 0; id < mod.GetNumProtos(); ++id)
        {
            if (id > 0) os << '\n';
            Zenith::Disassembler::Dump(mod, id, os);
        }
        return Diff::Trim(Diff::NormalizeNewlines(os.str()));
    }

}
