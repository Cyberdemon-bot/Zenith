#pragma once

namespace Zenith
{
    namespace
    {
        FORCE_INLINE void handle_EQ_I64(State& state, Instruction inst)
        {
            auto [a, b, c] = decode_args<Opcode::EQ_I64>(inst);
            Stack* stack = state.stack;
            stack->write(a, int64_t(stack->read<int64_t>(b) == stack->read<int64_t>(c)));
        }

        FORCE_INLINE void handle_LT_I64(State& state, Instruction inst)
        {
            auto [a, b, c] = decode_args<Opcode::LT_I64>(inst);
            Stack* stack = state.stack;
            stack->write(a, int64_t(stack->read<int64_t>(b) < stack->read<int64_t>(c)));
        }

        FORCE_INLINE void handle_LE_I64(State& state, Instruction inst)
        {
            auto [a, b, c] = decode_args<Opcode::LE_I64>(inst);
            Stack* stack = state.stack;
            stack->write(a, int64_t(stack->read<int64_t>(b) <= stack->read<int64_t>(c)));
        }

        FORCE_INLINE void handle_EQ_F64(State& state, Instruction inst)
        {
            auto [a, b, c] = decode_args<Opcode::EQ_F64>(inst);
            Stack* stack = state.stack;
            stack->write(a, int64_t(stack->read<double>(b) == stack->read<double>(c)));
        }

        FORCE_INLINE void handle_LT_F64(State& state, Instruction inst)
        {
            auto [a, b, c] = decode_args<Opcode::LT_F64>(inst);
            Stack* stack = state.stack;
            stack->write(a, int64_t(stack->read<double>(b) < stack->read<double>(c)));
        }

        FORCE_INLINE void handle_LE_F64(State& state, Instruction inst)
        {
            auto [a, b, c] = decode_args<Opcode::LE_F64>(inst);
            Stack* stack = state.stack;
            stack->write(a, int64_t(stack->read<double>(b) <= stack->read<double>(c)));
        }
    }
}