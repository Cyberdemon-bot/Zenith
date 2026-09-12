#pragma once

#include "Zenith/Base.h"

namespace Zenith 
{
    struct Stack
    {
        uint32_t* R32 = nullptr;
        uint64_t* R64 = nullptr;
        size_t size_r32 = NUM_R32;
        size_t size_r64 = NUM_R64;

        static constexpr std::size_t STACK_ALIGNMENT = 16;

        Stack(size_t cap32 = NUM_R32, size_t cap64 = NUM_R64)
            : size_r32(cap32), size_r64(cap64)
        {
            R32 = new (std::align_val_t{STACK_ALIGNMENT}) uint32_t[size_r32]{};
            R64 = new (std::align_val_t{STACK_ALIGNMENT}) uint64_t[size_r64]{};
        }

        ~Stack()
        {
            if (R32) ::operator delete[](R32, std::align_val_t{STACK_ALIGNMENT});
            if (R64) ::operator delete[](R64, std::align_val_t{STACK_ALIGNMENT});
        }

        Stack(const Stack&) = delete;
        Stack& operator=(const Stack&) = delete;

        Stack(Stack&& other) noexcept 
            : R32(other.R32), R64(other.R64), size_r32(other.size_r32), size_r64(other.size_r64)
        {
            other.R32 = nullptr;
            other.R64 = nullptr;
        }

        Stack& operator=(Stack&& other) noexcept
        {
            if (this != &other)
            {
                if (R32) ::operator delete[](R32, std::align_val_t{STACK_ALIGNMENT});
                if (R64) ::operator delete[](R64, std::align_val_t{STACK_ALIGNMENT});

                R32 = other.R32;
                R64 = other.R64;
                size_r32 = other.size_r32;
                size_r64 = other.size_r64;

                other.R32 = nullptr;
                other.R64 = nullptr;
            }
            return *this;
        }
    };

    struct State
    {   
        Stack* stack;
        Instruction* pc;

        template<typename T>
        FORCE_INLINE T read_R32(uint8_t index) const
        {
            return std::bit_cast<T>(stack->R32[index]);
        }

        FORCE_INLINE void write_R32(uint8_t index, int32_t val)
        {
            stack->R32[index] = std::bit_cast<uint32_t>(val);
        }

        FORCE_INLINE void write_R32(uint8_t index, float val)
        {
            stack->R32[index] = std::bit_cast<uint32_t>(val);
        }

        template<typename T>
        FORCE_INLINE T read_R64(uint8_t index) const
        {
            return std::bit_cast<T>(stack->R64[index]);
        }

        FORCE_INLINE void write_R64(uint8_t index, int64_t val)
        {
            stack->R64[index] = std::bit_cast<uint64_t>(val);
        }

        FORCE_INLINE void write_R64(uint8_t index, double val)
        {
            stack->R64[index] = std::bit_cast<uint64_t>(val);
        }
    };
}