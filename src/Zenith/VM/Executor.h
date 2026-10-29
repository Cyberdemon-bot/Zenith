#pragma once

#include "Zenith/Core/Base.h"
#include "Zenith/VM/State.h"
#include "Zenith/VM/Instruction.h"

#include <utility>

namespace Zenith {

    class Executor 
    {
    public:
        static void execute(State& state) noexcept;

    private:
        using StepFn = StepRet(*)(State*, const Instruction*, Value*, const Value*);

        template<uint32_t Slot>
        static StepRet step(State*, const Instruction*, Value*, const Value*);

        static const std::array<StepFn, DISPATCH_SIZE> STEP_TABLE;
    };
}