#pragma once

namespace Zenith 
{
    namespace 
    {
        FORCE_INLINE void handle_ADD_I32(State& state, Instruction inst)
        {
            int a = get_arg_A(inst);
            int b = get_arg_B(inst);
            int c = get_arg_C(inst);
            state.write_R32(a, state.read_R32<int32_t>(b) + state.read_R32<int32_t>(c));
        }

        FORCE_INLINE void handle_SUB_I32(State& state, Instruction inst)
        {
            int a = get_arg_A(inst);
            int b = get_arg_B(inst);
            int c = get_arg_C(inst);
            state.write_R32(a, state.read_R32<int32_t>(b) - state.read_R32<int32_t>(c));
        }

        FORCE_INLINE void handle_MUL_I32(State& state, Instruction inst)
        {
            int a = get_arg_A(inst);
            int b = get_arg_B(inst);
            int c = get_arg_C(inst);
            state.write_R32(a, state.read_R32<int32_t>(b) * state.read_R32<int32_t>(c));
        }

        FORCE_INLINE void handle_DIV_I32(State& state, Instruction inst)
        {
            int a = get_arg_A(inst);
            int b = get_arg_B(inst);
            int c = get_arg_C(inst);
            int32_t denom = state.read_R32<int32_t>(c);
            state.write_R32(a, denom != 0 ? state.read_R32<int32_t>(b) / denom : 0);
        }

        FORCE_INLINE void handle_MOD_I32(State& state, Instruction inst)
        {
            int a = get_arg_A(inst);
            int b = get_arg_B(inst);
            int c = get_arg_C(inst);
            int32_t denom = state.read_R32<int32_t>(c);
            state.write_R32(a, denom != 0 ? state.read_R32<int32_t>(b) % denom : 0);
        }

        FORCE_INLINE void handle_ADD_F32(State& state, Instruction inst)
        {
            int a = get_arg_A(inst);
            int b = get_arg_B(inst);
            int c = get_arg_C(inst);
            state.write_R32(a, state.read_R32<float>(b) + state.read_R32<float>(c));
        }

        FORCE_INLINE void handle_SUB_F32(State& state, Instruction inst)
        {
            int a = get_arg_A(inst);
            int b = get_arg_B(inst);
            int c = get_arg_C(inst);
            state.write_R32(a, state.read_R32<float>(b) - state.read_R32<float>(c));
        }

        FORCE_INLINE void handle_MUL_F32(State& state, Instruction inst)
        {
            int a = get_arg_A(inst);
            int b = get_arg_B(inst);
            int c = get_arg_C(inst);
            state.write_R32(a, state.read_R32<float>(b) * state.read_R32<float>(c));
        }

        FORCE_INLINE void handle_DIV_F32(State& state, Instruction inst)
        {
            int a = get_arg_A(inst);
            int b = get_arg_B(inst);
            int c = get_arg_C(inst);
            state.write_R32(a, state.read_R32<float>(b) / state.read_R32<float>(c));
        }
    }
}