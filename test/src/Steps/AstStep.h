#pragma once

#include "TestCase.h"

#include <string>
#include <string_view>
#include <vector>

class AstStep
{
public:
    struct ProfileData
    {
        float parseTimeMs = 0.0f;
        float dumpTimeMs = 0.0f;
        float totalTimeMs = 0.0f;
    };

    struct Result
    {
        std::string astTree;
        std::vector<ErrorInfo> errors;
        ProfileData profile;
    };

    static Result Run(std::string_view source);
};
