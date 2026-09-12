#pragma once
#include "Zenith/Base.h"

namespace Zenith 
{
    enum class Opcode : uint8_t 
    {
        OP_NOP = 0,
      
        OP_CAST32_F2I, 
        OP_ADD_I32,
        OP_SUB_I32,

        OP_CAST32_I2F, 
        OP_ADD_F32,
        OP_SUB_F32,

        OP_CAST64_F2I, 
        OP_ADD_I64,
        OP_SUB_I64,

        OP_CAST64_I2F, 
        OP_ADD_F64,
        OP_SUB_F64,
        
        OP_HALT = 255
    };

    constexpr Instruction make_inst(Opcode op, uint8_t dst = 0, uint8_t srcA = 0, uint8_t srcB = 0) 
    {
        return (static_cast<uint32_t>(op) << 24) |
               (static_cast<uint32_t>(dst) << 16) |
               (static_cast<uint32_t>(srcA) << 8)  |
               (static_cast<uint32_t>(srcB));
    }

    inline Opcode unpack_opcode(const Instruction& inst, uint8_t& dst, uint8_t& srcA, uint8_t& srcB)
    {
        dst = (inst >> 16) & 0xFF;
        srcA = (inst >> 8)  & 0xFF;
        srcB = inst & 0xFF;
        return static_cast<Opcode>((inst >> 24) & 0xFF);
    }
}