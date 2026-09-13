#pragma once

#include <new>
#include <bit>
#include <cstdint>
#include <cstddef>

namespace Zenith 
{
    using Instruction = uint32_t;

    constexpr std::size_t STACK_ALIGNMENT = 16;
    constexpr uint32_t NUM_R64 = 64;
    constexpr uint32_t SIZE_OP = 6;
    constexpr uint32_t SIZE_A  = 8;
    constexpr uint32_t SIZE_B  = 9;
    constexpr uint32_t SIZE_C  = 9;
    constexpr uint32_t SIZE_Bx = SIZE_C + SIZE_B; // 18 bit

    constexpr uint32_t POS_OP = 0;
    constexpr uint32_t POS_A  = POS_OP + SIZE_OP; // Bit 6
    constexpr uint32_t POS_C  = POS_A  + SIZE_A;  // Bit 14
    constexpr uint32_t POS_B  = POS_C  + SIZE_C;  // Bit 23
    constexpr uint32_t POS_Bx = POS_C;            // Bit 14

    constexpr uint32_t MAXARG_Bx  = (1U << SIZE_Bx) - 1U;                 // 262143
    constexpr int32_t  MAXARG_sBx = static_cast<int32_t>(MAXARG_Bx >> 1); // 131071 
    constexpr uint32_t MAXARG_A   = (1U << SIZE_A) - 1U;                  // 255
    constexpr uint32_t MAXARG_B   = (1U << SIZE_B) - 1U;                  // 511
    constexpr uint32_t MAXARG_C   = (1U << SIZE_C) - 1U;                  // 511
    constexpr uint16_t BIT_RK     = 1U << (SIZE_B - 1);                   // 256 

    #if defined(_MSC_VER)
        #define UNREACHABLE() __assume(0)
    #elif defined(__GNUC__) || defined(__clang__)
        #define UNREACHABLE() __builtin_unreachable()
    #else
        #define UNREACHABLE() std::abort()
    #endif

    #if defined(_MSC_VER)
        #define FORCE_INLINE __forceinline
    #elif defined(__GNUC__) || defined(__clang__)
        #define FORCE_INLINE inline __attribute__((always_inline)) constexpr
    #else
        #define FORCE_INLINE inline constexpr
    #endif
}