#pragma once

#include "TestCase.h"

#include <string>
#include <vector>

namespace Zenith {
    class Parser;
}

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
    static std::vector<ErrorInfo> CollectErrors(const Zenith::Parser& parser);
};
