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
            state.write_R64(a, state.read_R64<int64_t>(b) + state.read_R64<int64_t>(c));
        }

        FORCE_INLINE void handle_SUB_I64(State& state, Instruction inst)
        {
            int a = get_arg_A(inst);
            int b = get_arg_B(inst);
            int c = get_arg_C(inst);
            state.write_R64(a, state.read_R64<int64_t>(b) - state.read_R64<int64_t>(c));
        }

        FORCE_INLINE void handle_MUL_I64(State& state, Instruction inst)
        {
            int a = get_arg_A(inst);
            int b = get_arg_B(inst);
            int c = get_arg_C(inst);
            state.write_R64(a, state.read_R64<int64_t>(b) * state.read_R64<int64_t>(c));
        }

        FORCE_INLINE void handle_DIV_I64(State& state, Instruction inst)
        {
            int a = get_arg_A(inst);
            int b = get_arg_B(inst);
            int c = get_arg_C(inst);
            int64_t denom = state.read_R64<int64_t>(c);
            state.write_R64(a, denom != 0 ? state.read_R64<int64_t>(b) / denom : 0);
        }

        FORCE_INLINE void handle_ADD_F64(State& state, Instruction inst)
        {
            int a = get_arg_A(inst);
            int b = get_arg_B(inst);
            int c = get_arg_C(inst);
            state.write_R64(a, state.read_R64<double>(b) + state.read_R64<double>(c));
        }

        FORCE_INLINE void handle_SUB_F64(State& state, Instruction inst)
        {
            int a = get_arg_A(inst);
            int b = get_arg_B(inst);
            int c = get_arg_C(inst);
            state.write_R64(a, state.read_R64<double>(b) - state.read_R64<double>(c));
        }

        FORCE_INLINE void handle_MUL_F64(State& state, Instruction inst)
        {
            int a = get_arg_A(inst);
            int b = get_arg_B(inst);
            int c = get_arg_C(inst);
            state.write_R64(a, state.read_R64<double>(b) * state.read_R64<double>(c));
        }

        FORCE_INLINE void handle_DIV_F64(State& state, Instruction inst)
        {
            int a = get_arg_A(inst);
            int b = get_arg_B(inst);
            int c = get_arg_C(inst);
            state.write_R64(a, state.read_R64<double>(b) / state.read_R64<double>(c));
        }
    }
}