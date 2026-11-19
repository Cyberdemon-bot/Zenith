#pragma once

#include <cstdint>

namespace Zenith {

    struct CTType
    {
        enum class Kind 
        {
            Error = 0,
            Unknown, Var,
            Unit, Bool,
            I8, I16, I32, I64,
            U8, U16, U32, U64,
            F32, F64
        };

        Kind kind = Kind::Error;
        uint32_t var_id = 0; // meaningfull only when kind == Kind::Var

        static constexpr CTType Unknown() { return {Kind::Unknown, 0}; }
        static constexpr CTType Error()   { return {Kind::Error, 0}; }
        static constexpr CTType Unit()    { return {Kind::Unit, 0}; }
        static constexpr CTType Bool()    { return {Kind::Bool, 0}; }
        static constexpr CTType I8()      { return {Kind::I8, 0}; }
        static constexpr CTType I16()     { return {Kind::I16, 0}; }
        static constexpr CTType I32()     { return {Kind::I32, 0}; }
        static constexpr CTType I64()     { return {Kind::I64, 0}; }
        static constexpr CTType U8()      { return {Kind::U8, 0}; }
        static constexpr CTType U16()     { return {Kind::U16, 0}; }
        static constexpr CTType U32()     { return {Kind::U32, 0}; }
        static constexpr CTType U64()     { return {Kind::U64, 0}; }
        static constexpr CTType F32()     { return {Kind::F32, 0}; }
        static constexpr CTType F64()     { return {Kind::F64, 0}; }
        static constexpr CTType Var(uint32_t id) { return {Kind::Var, id}; }

        constexpr bool IsVar() const noexcept { return kind == Kind::Var; }
        explicit constexpr operator bool() const noexcept { return kind != Kind::Error && kind != Kind::Unknown; }
        constexpr bool operator==(const CTType& o) const noexcept
        {
            return kind == o.kind && (kind != Kind::Var || var_id == o.var_id);
        }
    };
    
}