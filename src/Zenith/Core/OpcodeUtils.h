#pragma once

#include "Zenith/Core/Base.h"
#include "Zenith/Core/Opcode.h"

namespace Zenith {
    
    FORCE_INLINE const OpInfo& get_opcode_info(Opcode op)
    {
        return OPCODE_INFO[static_cast<size_t>(op)];
    }

    FORCE_INLINE Instruction mask1(uint32_t n, uint32_t p)
    {
        return (~((~Instruction(0)) << n)) << p;
    }

    FORCE_INLINE Instruction mask0(uint32_t n, uint32_t p)
    {
        return ~mask1(n, p);
    }

    FORCE_INLINE Opcode get_opcode(Instruction inst)
    {
        return static_cast<Opcode>((inst >> POS_OP) & mask1(SIZE_OP, 0));
    }

    FORCE_INLINE int get_arg_A(Instruction inst)
    {
        return static_cast<int>((inst >> POS_A) & mask1(SIZE_A, 0));
    }

    FORCE_INLINE int get_arg_B(Instruction inst)
    {
        return static_cast<int>((inst >> POS_B) & mask1(SIZE_B, 0));
    }

    FORCE_INLINE int get_arg_C(Instruction inst)
    {
        return static_cast<int>((inst >> POS_C) & mask1(SIZE_C, 0));
    }

    FORCE_INLINE uint32_t get_arg_Bx(Instruction inst)
    {
        return static_cast<uint32_t>((inst >> POS_Bx) & mask1(SIZE_Bx, 0));
    }

    FORCE_INLINE int32_t get_arg_sBx(Instruction inst)
    {
        return static_cast<int32_t>(get_arg_Bx(inst)) - MAXARG_sBx;
    }

    FORCE_INLINE Instruction make_iABC(Opcode op, int a, int b, int c)
    {
        return (static_cast<Instruction>(op) << POS_OP)
             | (static_cast<Instruction>(a)  << POS_A)
             | (static_cast<Instruction>(b)  << POS_B)
             | (static_cast<Instruction>(c)  << POS_C);
    }

    FORCE_INLINE Instruction make_iABx(Opcode op, int a, uint32_t bx)
    {
        return (static_cast<Instruction>(op) << POS_OP)
             | (static_cast<Instruction>(a)  << POS_A)
             | (static_cast<Instruction>(bx) << POS_Bx);
    }

    FORCE_INLINE Instruction make_iAsBx(Opcode op, int a, int32_t sbx)
    {
        uint32_t bx = static_cast<uint32_t>(sbx + MAXARG_sBx);
        return make_iABx(op, a, bx);
    }

    FORCE_INLINE void set_opcode(Instruction& inst, Opcode op)
    {
        inst = (inst & mask0(SIZE_OP, POS_OP)) | ((static_cast<Instruction>(op) << POS_OP) & mask1(SIZE_OP, POS_OP));
    }

    FORCE_INLINE void set_arg_A(Instruction& inst, int a)
    {
        inst = (inst & mask0(SIZE_A, POS_A)) | ((static_cast<Instruction>(a) << POS_A) & mask1(SIZE_A, POS_A));
    }

    FORCE_INLINE void set_arg_B(Instruction& inst, int b)
    {
        inst = (inst & mask0(SIZE_B, POS_B)) | ((static_cast<Instruction>(b) << POS_B) & mask1(SIZE_B, POS_B));
    }

    FORCE_INLINE void set_arg_C(Instruction& inst, int c)
    {
        inst = (inst & mask0(SIZE_C, POS_C)) | ((static_cast<Instruction>(c) << POS_C) & mask1(SIZE_C, POS_C));
    }

    FORCE_INLINE void set_arg_Bx(Instruction& inst, uint32_t bx)
    {
        inst = (inst & mask0(SIZE_Bx, POS_Bx)) | ((static_cast<Instruction>(bx) << POS_Bx) & mask1(SIZE_Bx, POS_Bx));
    }

    FORCE_INLINE void set_arg_sBx(Instruction& inst, int32_t sbx)
    {
        set_arg_Bx(inst, static_cast<uint32_t>(sbx + MAXARG_sBx));
    }
}