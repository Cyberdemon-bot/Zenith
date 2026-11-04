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
    struct Result
    {
        std::string astTree;
        std::vector<ErrorInfo> errors;
    };

    static Result Run(std::string_view source);
    static std::vector<ErrorInfo> CollectErrors(const Zenith::Parser& parser);
};
