#pragma once

#include "TestCase.h"

#include <string>
#include <vector>

class TypeCheckerStep
{
public:
    struct ProfileData
    {
        float parseTimeMs = 0.0f;
        float checkTimeMs = 0.0f;
        float dumpTimeMs = 0.0f;
        float totalTimeMs = 0.0f;
    };

    struct Result
    {
        std::string typedAst;
        std::vector<ErrorInfo> errors;
        ProfileData profile;
    };

    static Result Run(std::string_view source);
};
