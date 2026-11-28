#pragma once

#include "TestCase.h"

#include "Zenith/Compiler/TypeChecker.h"

#include <string>
#include <vector>

class TypeCheckerStep
{
public:
    struct Result
    {
        Zenith::TypedAstView typed{};
        std::vector<ErrorInfo> errors;
        bool hasErrors = false;
        float timeMs = 0.0f;
    };

    Result Run(Zenith::AstView ast);
    static std::string Dump(Zenith::TypedAstView typed, float* outTimeMs = nullptr);

private:
    Zenith::TypeChecker m_Checker;
};
