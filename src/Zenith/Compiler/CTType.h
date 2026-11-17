#pragma once

#include <cstdint>

namespace Zenith {

    struct CTType
    {
        enum class Kind 
        {
            Error = 0,
            Unknown, Var,
            Unit, Bool, I64, F64
        };

        Kind kind = Kind::Error;
        uint32_t var_id = 0; // meaningfull only when kind == Kind::Var

        static constexpr CTType Unknown() { return {Kind::Unknown, 0}; }
        static constexpr CTType Error() { return {Kind::Error, 0}; }
        static constexpr CTType Unit() { return {Kind::Unit, 0}; }
        static constexpr CTType Bool() { return {Kind::Bool, 0}; }
        static constexpr CTType I64() { return {Kind::I64, 0}; }
        static constexpr CTType F64() { return {Kind::F64, 0}; }
        static constexpr CTType Var(uint32_t id) { return {Kind::Var, id}; }

        bool IsVar() const { return kind == Kind::Var; }
        bool operator==(const CTType& o) const 
        {
            return kind == o.kind && (kind != Kind::Var || var_id == o.var_id);
        }
    };
    
}