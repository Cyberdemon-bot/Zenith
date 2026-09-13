#pragma once

namespace Zenith 
{
    namespace 
    {
        FORCE_INLINE void handle_ADD_I64(State& state, Instruction inst)
        {
            int a = get_arg_A(inst);
            int b = get_arg_B(inst);
            int c = get_arg_C(inst);
            state.write(a, state.read<int64_t>(b) + state.read<int64_t>(c));
        }

        FORCE_INLINE void handle_SUB_I64(State& state, Instruction inst)
        {
            int a = get_arg_A(inst);
            int b = get_arg_B(inst);
            int c = get_arg_C(inst);
            state.write(a, state.read<int64_t>(b) - state.read<int64_t>(c));
        }

        FORCE_INLINE void handle_MUL_I64(State& state, Instruction inst)
        {
            int a = get_arg_A(inst);
            int b = get_arg_B(inst);
            int c = get_arg_C(inst);
            state.write(a, state.read<int64_t>(b) * state.read<int64_t>(c));
        }

        FORCE_INLINE void handle_DIV_I64(State& state, Instruction inst)
        {
            int a = get_arg_A(inst);
            int b = get_arg_B(inst);
            int c = get_arg_C(inst);
            int64_t denom = state.read<int64_t>(c);
            state.write(a, denom != 0 ? state.read<int64_t>(b) / denom : 0);
        }

        FORCE_INLINE void handle_ADD_F64(State& state, Instruction inst)
        {
            int a = get_arg_A(inst);
            int b = get_arg_B(inst);
            int c = get_arg_C(inst);
            state.write(a, state.read<double>(b) + state.read<double>(c));
        }

        FORCE_INLINE void handle_SUB_F64(State& state, Instruction inst)
        {
            int a = get_arg_A(inst);
            int b = get_arg_B(inst);
            int c = get_arg_C(inst);
            state.write(a, state.read<double>(b) - state.read<double>(c));
        }

        FORCE_INLINE void handle_MUL_F64(State& state, Instruction inst)
        {
            int a = get_arg_A(inst);
            int b = get_arg_B(inst);
            int c = get_arg_C(inst);
            state.write(a, state.read<double>(b) * state.read<double>(c));
        }

        FORCE_INLINE void handle_DIV_F64(State& state, Instruction inst)
        {
            int a = get_arg_A(inst);
            int b = get_arg_B(inst);
            int c = get_arg_C(inst);
            state.write(a, state.read<double>(b) / state.read<double>(c));
        }
    }
}