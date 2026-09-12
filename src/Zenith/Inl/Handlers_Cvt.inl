#pragma once

namespace Zenith 
{
    namespace 
    {
        FORCE_INLINE void handle_CVT_F32_I32(State& state, Instruction inst)
        {
            int a = get_arg_A(inst);
            int b = get_arg_B(inst);
            state.write_R32(a, static_cast<int32_t>(state.read_R32<float>(b)));
        }

        FORCE_INLINE void handle_CVT_I32_F32(State& state, Instruction inst)
        {
            int a = get_arg_A(inst);
            int b = get_arg_B(inst);
            state.write_R32(a, static_cast<float>(state.read_R32<int32_t>(b)));
        }

        FORCE_INLINE void handle_CVT_F64_I64(State& state, Instruction inst)
        {
            int a = get_arg_A(inst);
            int b = get_arg_B(inst);
            state.write_R64(a, static_cast<int64_t>(state.read_R64<double>(b)));
        }

        FORCE_INLINE void handle_CVT_I64_F64(State& state, Instruction inst)
        {
            int a = get_arg_A(inst);
            int b = get_arg_B(inst);
            state.write_R64(a, static_cast<double>(state.read_R64<int64_t>(b)));
        }
    }
}