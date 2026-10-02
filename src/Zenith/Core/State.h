#pragma once

#include "Zenith/Core/Base.h"
#include "Zenith/Object/Proto.h"

namespace Zenith 
{
    class Stack
    {
    public:
        Stack(size_t cap64 = NUM_R64)
            : size_r64(cap64)
        {
            R64 = new (std::align_val_t{STACK_ALIGNMENT}) uint64_t[size_r64]{};
        }

        ~Stack()
        {
            if (R64) ::operator delete[](R64, std::align_val_t{STACK_ALIGNMENT});
        }

        Stack(const Stack&) = delete;
        Stack& operator=(const Stack&) = delete;

        Stack(Stack&& other) noexcept 
            : R64(other.R64), size_r64(other.size_r64)
        {
            other.R64 = nullptr;
        }

        Stack& operator=(Stack&& other) noexcept
        {
            if (this != &other)
            {
                if (R64) ::operator delete[](R64, std::align_val_t{STACK_ALIGNMENT});

                R64 = other.R64;
                size_r64 = other.size_r64;

                other.R64 = nullptr;
            }
            return *this;
        }

        template<typename T>
        FORCE_INLINE T read(uint8_t index) const
        {
            return std::bit_cast<T>(R64[index]);
        }

        template<typename T>
        FORCE_INLINE void write(uint8_t index, T val)
        {
            R64[index] = std::bit_cast<uint64_t>(val);
        }

        FORCE_INLINE void move(uint8_t dst, uint8_t src)
        {
            R64[dst] = R64[src];
        }
    private:
        uint64_t* R64 = nullptr;
        size_t size_r64 = NUM_R64;
    };

    struct CallFrame
    {
        Instruction* pc;
        uint32_t base_reg;
        Proto* proto;
    };

    struct State
    {   
        Stack* stack;
        Instruction* pc;
        Proto proto;
    };
}