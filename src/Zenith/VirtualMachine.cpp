#include "Zenith/VirtualMachine.h"

namespace Zenith 
{
    void Machine::execute(State& state)
    {
        while (true)
        {
            Instruction inst = *state.pc++;
            uint8_t dst, srcA, srcB;
            Opcode op = unpack_opcode(inst, dst, srcA, srcB);

            switch (op)
            {
                case Opcode::OP_NOP: break;

                case Opcode::OP_CAST32_F2I:
                    state.write_R32(dst, static_cast<int32_t>(state.read_R32<float>(srcA)));
                    break;
                case Opcode::OP_ADD_I32:
                    state.write_R32(dst, state.read_R32<int32_t>(srcA) + state.read_R32<int32_t>(srcB));
                    break;
                case Opcode::OP_SUB_I32:
                    state.write_R32(dst, state.read_R32<int32_t>(srcA) - state.read_R32<int32_t>(srcB));
                    break;

                case Opcode::OP_CAST32_I2F:
                    state.write_R32(dst, static_cast<float>(state.read_R32<int32_t>(srcA)));
                    break;
                case Opcode::OP_ADD_F32:
                    state.write_R32(dst, state.read_R32<float>(srcA) + state.read_R32<float>(srcB));
                    break;
                case Opcode::OP_SUB_F32:
                    state.write_R32(dst, state.read_R32<float>(srcA) - state.read_R32<float>(srcB));
                    break;

                case Opcode::OP_CAST64_F2I:
                    state.write_R64(dst, static_cast<int64_t>(state.read_R64<double>(srcA)));
                    break;
                case Opcode::OP_ADD_I64:
                    state.write_R64(dst, state.read_R64<int64_t>(srcA) + state.read_R64<int64_t>(srcB));
                    break;
                case Opcode::OP_SUB_I64:
                    state.write_R64(dst, state.read_R64<int64_t>(srcA) - state.read_R64<int64_t>(srcB));
                    break;

                case Opcode::OP_CAST64_I2F:
                    state.write_R64(dst, static_cast<double>(state.read_R64<int64_t>(srcA)));
                    break;
                case Opcode::OP_ADD_F64:
                    state.write_R64(dst, state.read_R64<double>(srcA) + state.read_R64<double>(srcB));
                    break;
                case Opcode::OP_SUB_F64:
                    state.write_R64(dst, state.read_R64<double>(srcA) - state.read_R64<double>(srcB));
                    break;

                case Opcode::OP_HALT: 
                    return;
            }
        }
    }
}