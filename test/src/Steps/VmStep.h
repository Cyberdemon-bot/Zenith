#pragma once

#include "TestCase.h"

#include <string>
#include <string_view>
#include <vector>

class VmStep
{
public:
    struct Result
    {
        bool success = false;
        std::string errorMessage;
        std::string actualVm;
        std::string actualDisasm;
    };

    struct RegisterExpectation
    {
        uint8_t regIndex = 0;
        std::string expectedStr;
        std::string type; // e.g. "I32", "F32", "U32", "I64", "F64", "U64"
    };

    static Result RunAsm(std::string_view asmSource, const std::string& expectedVmBlock);
    static std::vector<RegisterExpectation> ParseExpectations(const std::string& expectedBlock);
};
