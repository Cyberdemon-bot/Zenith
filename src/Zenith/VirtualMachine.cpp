#include "Zenith/OpcodeUtils.h"
#include "Zenith/VirtualMachine.h"

namespace Zenith 
{
    void Machine::execute(State& state)
    {
        while (true)
        {
            Instruction inst = *state.pc++;
            Opcode op = get_opcode(inst);

            int a = get_arg_A(inst); 
            int b = get_arg_B(inst); 
            int c = get_arg_C(inst); 

            switch (op)
            {
                case Opcode::NOP: break;

                case Opcode::CVT_F32_I32:
                {
                    state.write_R32(a, static_cast<int32_t>(state.read_R32<float>(b)));
                    break;
                }
                case Opcode::ADD_I32:
                {
                    state.write_R32(a, state.read_R32<int32_t>(b) + state.read_R32<int32_t>(c));
                    break;
                }
                case Opcode::SUB_I32:
                {
                    state.write_R32(a, state.read_R32<int32_t>(b) - state.read_R32<int32_t>(c));
                    break;
                }
                case Opcode::CVT_I32_F32:
                {
                    state.write_R32(a, static_cast<float>(state.read_R32<int32_t>(b)));
                    break;
                }
                case Opcode::ADD_F32:
                {
                    state.write_R32(a, state.read_R32<float>(b) + state.read_R32<float>(c));
                    break;
                }
                case Opcode::SUB_F32:
                {
                    state.write_R32(a, state.read_R32<float>(b) - state.read_R32<float>(c));
                    break;
                }
                case Opcode::CVT_F64_I64:
                {
                    state.write_R64(a, static_cast<int64_t>(state.read_R64<double>(b)));
                    break;
                }
                case Opcode::ADD_I64:
                {
                    state.write_R64(a, state.read_R64<int64_t>(b) + state.read_R64<int64_t>(c));
                    break;
                }
                case Opcode::SUB_I64:
                {
                    state.write_R64(a, state.read_R64<int64_t>(b) - state.read_R64<int64_t>(c));
                    break;
                }
                case Opcode::CVT_I64_F64:
                {
                    state.write_R64(a, static_cast<double>(state.read_R64<int64_t>(b)));
                    break;
                }
                case Opcode::ADD_F64:
                {
                    state.write_R64(a, state.read_R64<double>(b) + state.read_R64<double>(c));
                    break;
                }
                case Opcode::SUB_F64:
                {
                    state.write_R64(a, state.read_R64<double>(b) - state.read_R64<double>(c));
                    break;
                }

                case Opcode::HALT: return;
                default: ZENITH_UNREACHABLE();
            }
        }
    }
}