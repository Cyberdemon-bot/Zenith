#pragma once

namespace Zenith 
{
    namespace 
    {
        FORCE_INLINE void handle_CVT_F64_I64(State& state, Instruction inst)
        {
            int a = get_arg_A(inst);
            int b = get_arg_B(inst);
            state.write(a, static_cast<int64_t>(state.read<double>(b)));
        }

        FORCE_INLINE void handle_CVT_I64_F64(State& state, Instruction inst)
        {
            int a = get_arg_A(inst);
            int b = get_arg_B(inst);
            state.write(a, static_cast<double>(state.read<int64_t>(b)));
        }
    }
}