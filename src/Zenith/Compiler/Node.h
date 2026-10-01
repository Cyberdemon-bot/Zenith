#pragma once

namespace Zenith {

    enum class NodeType : uint16_t 
    {
        Error = 0,

        Integer,
        Decimal,
        String,
        Identifier,
        Label,

        UnaryExpr,
        BinaryExpr,
        CallExpr,
        IndexExpr,
        FieldAccess,
        BorrowExpr,
        NewExpr,
        LabeledExpr,
        BlockExpr,
        IfExpr,
        MatchExpr,
        MatchArm,

        ExprStmt,
        LetStmt,
        ReturnStmt,
        BreakStmt,
        ContinueStmt,

        TypeAnnotation, 
        ParamDecl,
        FieldDecl,
        FuncDecl,
        StructDecl,
        ClassDecl,
        Program
    };

    struct Node
    {
        NodeType type;
        uint32_t token;
        union Data
        {
            int64_t i64; double f64;
            struct { uint32_t lhs, rhs; } indicies;
            struct { uint32_t start; uint32_t count; } list;
        } data;
    };
}