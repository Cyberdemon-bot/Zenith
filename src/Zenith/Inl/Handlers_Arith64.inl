#pragma once

#include "Zenith/Inl/Handlers_Utils.inl"
namespace Zenith 
{
    namespace 
    {
        FORCE_INLINE void handle_ADD_I64(State& state, Instruction inst)
        {
            Stack* stack = state.stack;
            auto [a, b, c] = decode_args<Opcode::ADD_I64>(inst);
            stack->write(a, stack->read<int64_t>(b) + stack->read<int64_t>(c));
        }

        FORCE_INLINE void handle_SUB_I64(State& state, Instruction inst)
        {
            Stack* stack = state.stack;
            auto [a, b, c] = decode_args<Opcode::SUB_I64>(inst);
            stack->write(a, stack->read<int64_t>(b) - stack->read<int64_t>(c));
        }

        FORCE_INLINE void handle_MUL_I64(State& state, Instruction inst)
        {
            Stack* stack = state.stack;
            auto [a, b, c] = decode_args<Opcode::MUL_I64>(inst);
            stack->write(a, stack->read<int64_t>(b) * stack->read<int64_t>(c));
        }

        FORCE_INLINE void handle_DIV_I64(State& state, Instruction inst)
        {
            Stack* stack = state.stack;
            auto [a, b, c] = decode_args<Opcode::DIV_I64>(inst);
            int64_t denom = stack->read<int64_t>(c);
            stack->write(a, denom != 0 ? stack->read<int64_t>(b) / denom : 0);
        }

        FORCE_INLINE void handle_ADD_F64(State& state, Instruction inst)
        {
            Stack* stack = state.stack;
            auto [a, b, c] = decode_args<Opcode::ADD_F64>(inst);
            stack->write(a, stack->read<double>(b) + stack->read<double>(c));
        }

        FORCE_INLINE void handle_SUB_F64(State& state, Instruction inst)
        {
            Stack* stack = state.stack;
            auto [a, b, c] = decode_args<Opcode::SUB_F64>(inst);
            stack->write(a, stack->read<double>(b) - stack->read<double>(c));
        }

        FORCE_INLINE void handle_MUL_F64(State& state, Instruction inst)
        {
            Stack* stack = state.stack;
            auto [a, b, c] = decode_args<Opcode::MUL_F64>(inst);
            stack->write(a, stack->read<double>(b) * stack->read<double>(c));
        }

        FORCE_INLINE void handle_DIV_F64(State& state, Instruction inst)
        {
            Stack* stack = state.stack;
            auto [a, b, c] = decode_args<Opcode::DIV_F64>(inst);
            stack->write(a, stack->read<double>(b) / stack->read<double>(c));
        }
    }
}