#include "Zenith/Core/VirtualMachine.h"
#include "Zenith/Core/OpcodeUtils.h"
#include "Zenith/Inl/Handlers_Data.inl"
#include "Zenith/Inl/Handlers_Cvt.inl"
#include "Zenith/Inl/Handlers_Arith64.inl"
#include "Zenith/Inl/Handlers_Cmp.inl"
#include "Zenith/Inl/Handlers_Jump.inl"

#define HANDLE(OP) { handle_##OP(state, inst); break; }

namespace Zenith 
{
    void Machine::execute(State& state)
    {
        Stack& stack = *state.stack;
        state.pc = state.proto.code.data();
        while (true)
        {
            Instruction inst = *state.pc++;
            Opcode op = get_opcode(inst);

            switch (op)
            {
                case Opcode::NOP: break;

                case Opcode::MOVE:  HANDLE(MOVE)
                case Opcode::LOADI: HANDLE(LOADI)
                case Opcode::LOADF: HANDLE(LOADF)

                case Opcode::CVT_F64_I64: HANDLE(CVT_F64_I64)
                case Opcode::CVT_I64_F64: HANDLE(CVT_I64_F64)

                case Opcode::ADD_I64: HANDLE(ADD_I64)
                case Opcode::SUB_I64: HANDLE(SUB_I64)
                case Opcode::MUL_I64: HANDLE(MUL_I64)
                case Opcode::DIV_I64: HANDLE(DIV_I64)

                case Opcode::ADD_F64: HANDLE(ADD_F64)
                case Opcode::SUB_F64: HANDLE(SUB_F64)
                case Opcode::MUL_F64: HANDLE(MUL_F64)
                case Opcode::DIV_F64: HANDLE(DIV_F64)

                case Opcode::EQ_I64: HANDLE(EQ_I64)
                case Opcode::LT_I64: HANDLE(LT_I64)
                case Opcode::LE_I64: HANDLE(LE_I64)
                case Opcode::EQ_F64: HANDLE(EQ_F64)
                case Opcode::LT_F64: HANDLE(LT_F64)
                case Opcode::LE_F64: HANDLE(LE_F64)

                case Opcode::JMP:      HANDLE(JMP)
                case Opcode::JMPIF:    HANDLE(JMPIF)
                case Opcode::JMPIFNOT: HANDLE(JMPIFNOT)

                case Opcode::HALT: return;
                default: UNREACHABLE();
            }
        }
    }
}