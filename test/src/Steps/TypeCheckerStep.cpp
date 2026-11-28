#include "TypeCheckerStep.h"
#include "Diff.h"
#include "StepUtils.h"

#include "Zenith/Compiler/Dumper.h"
#include "Zenith/Core/ProfileTimer.h"

TypeCheckerStep::Result TypeCheckerStep::Run(Zenith::AstView ast)
{
    Result res;
    {
        ZN_PROFILE_SCOPE(&res.timeMs);
        res.typed = m_Checker.Validate(ast);
    }
    res.hasErrors = m_Checker.HasErrors();
    res.errors = StepUtils::CollectDiagnostics(ast, m_Checker.Diagnostics());
    return res;
}

std::string TypeCheckerStep::Dump(Zenith::TypedAstView typed, float* outTimeMs)
{
    float elapsed = 0.0f;
    std::string dumped;
    {
        ZN_PROFILE_SCOPE(&elapsed);
        dumped = Diff::Trim(Diff::NormalizeNewlines(
            Zenith::Dumper::DumpTypedAst(typed.ast, typed.node_types)));
    }
    if (outTimeMs) *outTimeMs = elapsed;
    return dumped;
}
