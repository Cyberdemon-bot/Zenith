#pragma once
#include "Zenith/State.h"
#include "Zenith/Opcode.h"

namespace Zenith 
{
    class Machine
    {
    public:
        void execute(State& state);
    };
}