#pragma once

namespace Zenith 
{
    namespace 
    {
        FORCE_INLINE void handle_MOVE(State& state, Instruction inst)
        {
            int a = get_arg_A(inst);
            int b = get_arg_B(inst);
            state.move(a, b);
        }

        FORCE_INLINE void handle_LOADI(State& state, Instruction inst)
        {
            int a = get_arg_A(inst);
            int32_t sbx = get_arg_sBx(inst);
            state.write(a, static_cast<int64_t>(sbx));
        }
    }
}