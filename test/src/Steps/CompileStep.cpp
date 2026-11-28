#include "CompileStep.h"
#include "StepUtils.h"

#include "Zenith/Core/ProfileTimer.h"

CompileStep::Result CompileStep::Run(Zenith::TypedAstView typed)
{
    Result res;
    auto mod = Zenith::CreateRef<Zenith::Module>();
    bool ok = false;
    {
        ZN_PROFILE_SCOPE(&res.timeMs);
        ok = m_CodeGen.Generate(typed, *mod);
    }
    res.hasErrors = !ok || m_CodeGen.HasErrors();
    res.errors = StepUtils::CollectDiagnostics(typed.ast, m_CodeGen.Diagnostics());
    if (!res.hasErrors)
        res.module = std::move(mod);
    return res;
}

std::string CompileStep::DumpDisasm(const Zenith::Module& mod, float* outTimeMs)
{
    float elapsed = 0.0f;
    std::string dumped;
    {
        ZN_PROFILE_SCOPE(&elapsed);
        dumped = StepUtils::DumpDisasm(mod);
    }
    if (outTimeMs) *outTimeMs = elapsed;
    return dumped;
}
