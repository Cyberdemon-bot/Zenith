#pragma once

namespace Zenith {

    enum class NodeType : uint16_t 
    {
        Error = 0,

        Integer, Decimal, String, Bool, Null,
        Tuple,
        Identifier,
        TypedIdentifier,
        Label,

        UnaryExpr,
        BinaryExpr,
        CallExpr,
        IndexExpr,
        FieldAccess,
        BorrowExpr,
        NewExpr,
        LabeledBlockExpr,
        BlockExpr,
        IfExpr,
        MatchExpr,
        MatchArm,

        ExprStmt,
        LetStmt,
        ReturnStmt,
        BreakStmt,
        ContinueStmt,
        AssignStmt, 

        TypeAnnotation, 
        ArgList,
        FuncDecl,
        StructDecl,
        ClassDecl,
        Program
    };

    struct Node
    {
        NodeType type;
        uint32_t start_token, end_token;
        union Data
        {
            int64_t i64; double f64;
            struct { uint32_t lhs, rhs; } indicies;
            struct { uint32_t start; uint32_t count; } list;
        } data;
    };
}