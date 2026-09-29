#pragma once

namespace Zenith {

    enum class NodeType : uint16_t 
    {
        Error = 0,
        BinaryExpr,
        UnaryExpr,
        Integer,
        Decimal,
        Identifier,
        Tuple,
        Block
    };

    struct Node
    {
        NodeType type;
        uint32_t token;
        union Data
        {
            int64_t i64; double f64;
            struct { uint32_t lhs, rhs; } u32;
        } data;
    };
}