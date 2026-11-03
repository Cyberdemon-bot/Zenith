#pragma once

#include "Zenith/Core/Base.h"
#include "Zenith/VM/Opcode.h"

#include <array>
#include <cstdint>

namespace Zenith {

    class State;
    struct Instruction;
    union Value;

    class Executor 
    {
    public:
        static void Execute(State& state) noexcept;

    private:
        using StepFn = StepRet(*)(State*, const Instruction*, Value*, const Value*);

        template<uint32_t Slot>
        static StepRet step(State*, const Instruction*, Value*, const Value*);

        static const std::array<StepFn, DISPATCH_SIZE> STEP_TABLE;
    };
}