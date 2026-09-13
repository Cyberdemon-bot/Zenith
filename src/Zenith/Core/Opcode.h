#pragma once

#include "Zenith/Core/Base.h"

namespace Zenith 
{
    enum class OpMode : uint8_t { iABC, iABx, iAsBx };

    enum class Opcode : uint8_t 
    {
        NOP = 0,
        MOVE, 
        LOADI, LOADF,   

        // 64 bit
        CVT_F64_I64, ADD_I64, SUB_I64, MUL_I64, DIV_I64,
        CVT_I64_F64, ADD_F64, SUB_F64, MUL_F64, DIV_F64,
        
        HALT,
        COUNT
    };

    enum class OpArgMask : uint8_t // use for b and c
    {
        OpArgN = 0, // no use
        OpArgU,     // unsigned
        OpArgR,     // register 
        OpArgK      // constant index
    };

    struct OpInfo 
    {
        OpMode mode: 2;     // iABC, iABx, iAsBx
        OpArgMask arg_c: 2; 
        OpArgMask arg_b: 2; 
        bool sets_reg : 1;  // should vm set reg A?
        bool is_test: 1;    // is condition test?
    };

    constexpr OpInfo OPCODE_INFO[static_cast<size_t>(Opcode::COUNT)] = {
        /* NOP         */ { OpMode::iABC,  OpArgMask::OpArgN, OpArgMask::OpArgN, false, false },
        /* MOVE        */ { OpMode::iABC,  OpArgMask::OpArgN, OpArgMask::OpArgR, true,  false },
        /* LOADI       */ { OpMode::iABx,  OpArgMask::OpArgN, OpArgMask::OpArgN, true,  false },
        /* LOADF       */ { OpMode::iABx,  OpArgMask::OpArgN, OpArgMask::OpArgN, true,  false },

        /* CVT_F64_I64 */ { OpMode::iABC,  OpArgMask::OpArgN, OpArgMask::OpArgR, true,  false },
        /* ADD_I64     */ { OpMode::iABC,  OpArgMask::OpArgR, OpArgMask::OpArgR, true,  false },
        /* SUB_I64     */ { OpMode::iABC,  OpArgMask::OpArgR, OpArgMask::OpArgR, true,  false },
        /* MUL_I64     */ { OpMode::iABC,  OpArgMask::OpArgR, OpArgMask::OpArgR, true,  false },
        /* DIV_I64     */ { OpMode::iABC,  OpArgMask::OpArgR, OpArgMask::OpArgR, true,  false },

        /* CVT_I64_F64 */ { OpMode::iABC,  OpArgMask::OpArgN, OpArgMask::OpArgR, true,  false },
        /* ADD_F64     */ { OpMode::iABC,  OpArgMask::OpArgR, OpArgMask::OpArgR, true,  false },
        /* SUB_F64     */ { OpMode::iABC,  OpArgMask::OpArgR, OpArgMask::OpArgR, true,  false },
        /* MUL_F64     */ { OpMode::iABC,  OpArgMask::OpArgR, OpArgMask::OpArgR, true,  false },
        /* DIV_F64     */ { OpMode::iABC,  OpArgMask::OpArgR, OpArgMask::OpArgR, true,  false },

        /* HALT        */ { OpMode::iABC,  OpArgMask::OpArgN, OpArgMask::OpArgN, false, false }
    };
}