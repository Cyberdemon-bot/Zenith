#pragma once

namespace Zenith
{
    namespace
    {
        FORCE_INLINE void handle_JMP(State& state, Instruction inst)
        {
            int32_t sbx = get_arg_sBx(inst);
            state.pc += sbx;
        }

        FORCE_INLINE void handle_JMPIF(State& state, Instruction inst)
        {
            auto [a, sbx] = decode_args<Opcode::JMPIF>(inst);
            if (state.stack->read<int64_t>(a) != 0)
            {
                state.pc += sbx;
            }
        }

        FORCE_INLINE void handle_JMPIFNOT(State& state, Instruction inst)
        {
            auto [a, sbx] = decode_args<Opcode::JMPIFNOT>(inst);
            if (state.stack->read<int64_t>(a) == 0)
            {
                state.pc += sbx;
            }
        }
    }
}