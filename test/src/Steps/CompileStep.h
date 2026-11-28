#pragma once

#include "TestCase.h"

#include "Zenith/Compiler/CodeGen.h"
#include "Zenith/Core/Base.h"
#include "Zenith/VM/Proto.h"

#include <string>
#include <vector>

class CompileStep
{
public:
    struct Result
    {
        Zenith::Ref<const Zenith::Module> module;
        std::vector<ErrorInfo> errors;
        bool hasErrors = false;
        float timeMs = 0.0f;
    };

    Result Run(Zenith::TypedAstView typed);
    static std::string DumpDisasm(const Zenith::Module& mod, float* outTimeMs = nullptr);

private:
    Zenith::CodeGen m_CodeGen;
};
