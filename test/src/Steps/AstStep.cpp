#include "AstStep.h"
#include "Diff.h"
#include "StepUtils.h"

#include "Zenith/Compiler/Dumper.h"
#include "Zenith/Core/ProfileTimer.h"

AstStep::Result AstStep::Run(std::string_view source)
{
    Result res;
    {
        ZN_PROFILE_SCOPE(&res.timeMs);
        res.ast = m_Parser.ParseProgram(source);
    }
    res.hasErrors = m_Parser.HasErrors();
    res.errors = StepUtils::CollectDiagnostics(res.ast, m_Parser.Diagnostics());
    return res;
}

std::string AstStep::Dump(Zenith::AstView ast, float* outTimeMs)
{
    float elapsed = 0.0f;
    std::string dumped;
    {
        ZN_PROFILE_SCOPE(&elapsed);
        dumped = Diff::Trim(Diff::NormalizeNewlines(Zenith::Dumper::DumpTreeAst(ast)));
    }
    if (outTimeMs) *outTimeMs = elapsed;
    return dumped;
}
