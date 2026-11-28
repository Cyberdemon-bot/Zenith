#pragma once

#include "TestCase.h"

#include "Zenith/Core/Base.h"
#include "Zenith/VM/Proto.h"
#include "Zenith/VM/State.h"

#include <string>
#include <string_view>
#include <vector>

class VmStep
{
public:
    struct AssembleResult
    {
        Zenith::Ref<const Zenith::Module> module;
        std::string errorMessage;
        bool success = false;
        float timeMs = 0.0f;
    };

    struct ExecResult
    {
        std::string actualVm;
        std::string errorMessage;
        bool success = false;
        float timeMs = 0.0f;
    };

    struct RegisterExpectation
    {
        uint8_t regIndex = 0;
        std::string expectedStr;
        std::string type; // e.g. "I32", "F32", "U32", "I64", "F64", "U64", "BOOL"
    };

    AssembleResult Assemble(std::string_view asmSource);
    ExecResult Execute(const Zenith::Ref<const Zenith::Module>& module, const std::string& expectedVmBlock);

    static std::vector<RegisterExpectation> ParseExpectations(const std::string& expectedBlock);

private:
    Zenith::State m_State;
};
