#pragma once

#include "Zenith/Core/Base.h"

#include <bit>
#include <cstdint>

namespace Zenith {

    enum class Type : uint8_t 
    {
        U32, I32, F32,
        U64, I64, F64,
        REF,

        None, Count
    };

    constexpr uint32_t TYPE_COUNT = static_cast<uint32_t>(Type::Count);

    union Value 
    {
        uint32_t  u32; int32_t i32; float  f32;
        uint64_t  u64; int64_t i64; double f64;
        uintptr_t ref;

        Value() : u64(0) {}

        template<Type T>
        constexpr auto& get_as() noexcept 
        {
            if      constexpr (T == Type::U32) return u32;
            else if constexpr (T == Type::I32) return i32;
            else if constexpr (T == Type::F32) return f32;
            else if constexpr (T == Type::U64) return u64;
            else if constexpr (T == Type::I64) return i64;
            else if constexpr (T == Type::F64) return f64;
            else if constexpr (T == Type::REF) return ref;
            else static_assert(always_false_v<T>, "Unsupported Value Type");
        }

        template<Type T>
        constexpr const auto& get_as() const noexcept 
        {  
            if      constexpr (T == Type::U32) return u32;
            else if constexpr (T == Type::I32) return i32;
            else if constexpr (T == Type::F32) return f32;
            else if constexpr (T == Type::U64) return u64;
            else if constexpr (T == Type::I64) return i64;
            else if constexpr (T == Type::F64) return f64;
            else if constexpr (T == Type::REF) return ref;
            else static_assert(always_false_v<T>, "Unsupported Value Type");
        }

        template<Type T, typename V>
        constexpr void set_as(V val) noexcept 
        {
            if constexpr (T == Type::U32 || T == Type::I32) 
            {
                u64 = static_cast<uint32_t>(val);
            } 
            else if constexpr (T == Type::F32) 
            {
                u64 = static_cast<uint64_t>(std::bit_cast<uint32_t>(static_cast<float>(val)));
            } 
            else if constexpr (T == Type::U64 || T == Type::REF) 
            {
                u64 = static_cast<uint64_t>(val);
            } 
            else if constexpr (T == Type::I64) 
            {
                i64 = static_cast<int64_t>(val);
            } 
            else if constexpr (T == Type::F64) 
            {
                f64 = static_cast<double>(val);
            } 
            else 
            {
                static_assert(always_false_v<T>, "Unsupported Value Type");
            }
        }
    };

    static_assert(sizeof(Value)  == 8, "Value must be exactly 8 bytes");
    static_assert(alignof(Value) == 8, "Value must be 8-byte aligned");
}