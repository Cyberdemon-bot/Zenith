#pragma once

#include "Zenith/Inl/Handlers_Utils.inl"
namespace Zenith 
{
    namespace 
    {
        FORCE_INLINE void handle_CVT_F64_I64(State& state, Instruction inst)
        {
            Stack* stack = state.stack;
            auto [a, b] = decode_args<Opcode::CVT_F64_I64>(inst);
            stack->write(a, static_cast<int64_t>(stack->read<double>(b)));
        }

        FORCE_INLINE void handle_CVT_I64_F64(State& state, Instruction inst)
        {
            Stack* stack = state.stack;
            auto [a, b] = decode_args<Opcode::CVT_I64_F64>(inst);
            stack->write(a, static_cast<double>(stack->read<int64_t>(b)));
        }
    }
}