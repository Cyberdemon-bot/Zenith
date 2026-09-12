#pragma once

#include "Zenith/Base.h"

namespace Zenith 
{
    struct Stack
    {
        alignas(16) uint32_t R32[NUM_R32];
        alignas(16) uint64_t R64[NUM_R64];
    };

    struct State
    {   
        Stack* stack;
        Instruction* pc;

        template<typename T>
        inline T read_R32(uint8_t index) const
        {
            return std::bit_cast<T>(stack->R32[index]);
        }

        inline void write_R32(uint8_t index, int32_t val)
        {
            stack->R32[index] = std::bit_cast<uint32_t>(val);
        }

        inline void write_R32(uint8_t index, float val)
        {
            stack->R32[index] = std::bit_cast<uint32_t>(val);
        }

        template<typename T>
        inline T read_R64(uint8_t index) const
        {
            return std::bit_cast<T>(stack->R64[index]);
        }

        inline void write_R64(uint8_t index, int64_t val)
        {
            stack->R64[index] = std::bit_cast<uint64_t>(val);
        }

        inline void write_R64(uint8_t index, double val)
        {
            stack->R64[index] = std::bit_cast<uint64_t>(val);
        }
    };
}