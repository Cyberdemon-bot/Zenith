#pragma once

#include <cstdint>

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
        CallExpr, ArgList,
        IndexExpr,
        FieldAccess,
        BorrowExpr,
        NewExpr,
        LabeledBlockExpr,
        BlockExpr,
        IfExpr,
        MatchExpr, MatchArm,

        ExprStmt,
        LetStmt,
        ReturnStmt,
        BreakStmt,
        ContinueStmt,
        AssignStmt, 

        TypeAnnotation, 
        FuncDecl, FuncSign, FuncArgDecl,
        StructDecl,
        ClassDecl,
        Program
    };

    enum TypeAnnotationFlags : uint8_t
    {
        TYPE_FLAG_BORROW = 1 << 0,
        TYPE_FLAG_MUT    = 1 << 1,
    };

    struct Node
    {
        NodeType type;
        uint32_t start_token, end_token;
        union Data
        {
            int64_t  i64;
            double   f64;
            uint32_t child;

            struct { uint32_t start, count; }                   list;
            struct { uint32_t lhs, op, rhs; }                   binary;
            struct { uint32_t op, operand; }                    unary;
            struct { uint32_t cond, then_branch, else_branch; } if_expr;

            struct { uint32_t first, second; }                  pair;
            struct { uint32_t target, member; }                 access;
            struct { uint32_t callee, args; }                   call;
            struct { uint32_t label, body; }                    labeled;
            struct { uint32_t ident, type; uint8_t is_mut; }    typed_ident;
            struct { uint32_t operand; uint8_t is_mut; }        borrow;
            struct { uint32_t ident; uint8_t flags; }           type_annot;

            struct { uint32_t name, params, ret_type; }         func_sign;
            struct { uint32_t sign, body; }                     func_decl;
            struct { uint32_t ident, type, default_val; }       func_arg;
            struct { uint32_t name, fields; }                   struct_decl;
            struct { uint32_t target, value; }                  let;

            struct { uint32_t pattern, guard, body; }           match_arm;

            struct { uint32_t type, args; }                     new_expr;
            struct { uint32_t label, value; }                   break_stmt;
        } data;
    };
}