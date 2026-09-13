#pragma once

#include "Zenith/Inl/Handlers_Utils.inl"
namespace Zenith 
{
    namespace 
    {
        FORCE_INLINE void handle_MOVE(State& state, Instruction inst)
        {
            Stack* stack = state.stack;
            auto [a, b] = decode_args<Opcode::MOVE>(inst);
            stack->move(a, b);
        }

        FORCE_INLINE void handle_LOADI(State& state, Instruction inst)
        {
            Stack* stack = state.stack;
            auto [a, bx] = decode_args<Opcode::LOADF>(inst);
            uint64_t raw = state.proto.numeric_constants[bx];
            int64_t value = std::bit_cast<int64_t>(raw);
            stack->write(a, value);
        }

        FORCE_INLINE void handle_LOADF(State& state, Instruction inst)
        {
            Stack* stack = state.stack;
            auto [a, bx] = decode_args<Opcode::LOADF>(inst);
            uint64_t raw = state.proto.numeric_constants[bx];
            double value = std::bit_cast<double>(raw);
            stack->write(a, value);
        }
    }
}