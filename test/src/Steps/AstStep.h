#pragma once

#include "TestCase.h"

#include "Zenith/Compiler/Parser.h"

#include <string>
#include <string_view>
#include <vector>

class AstStep
{
public:
    struct Result
    {
        Zenith::AstView ast{};
        std::vector<ErrorInfo> errors;
        bool hasErrors = false;
        float timeMs = 0.0f;
    };

    Result Run(std::string_view source);
    static std::string Dump(Zenith::AstView ast, float* outTimeMs = nullptr);

private:
    Zenith::Parser m_Parser;
};
