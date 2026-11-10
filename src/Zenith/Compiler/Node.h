#pragma once

#include <cstdint>
#include <string_view>

namespace Zenith {

    constexpr uint32_t ERROR_NODE = 0;

    enum class Operator : uint8_t
    {
        Add,
        Sub,
        Mul,
        Div,
        Mod,

        Eq,
        Ne,
        Lt,
        Lte,
        Gt,
        Gte,

        And,
        Or,

        Assign,

        Not,
        Neg
    };

    constexpr std::string_view OperatorToString(Operator op) noexcept
    {
        switch (op)
        {
            case Operator::Add:    return "+";
            case Operator::Sub:    return "-";
            case Operator::Mul:    return "*";
            case Operator::Div:    return "/";
            case Operator::Mod:    return "%";
            case Operator::Eq:     return "==";
            case Operator::Ne:     return "!=";
            case Operator::Lt:     return "<";
            case Operator::Lte:    return "<=";
            case Operator::Gt:     return ">";
            case Operator::Gte:    return ">=";
            case Operator::And:    return "and";
            case Operator::Or:     return "or";
            case Operator::Assign: return "=";
            case Operator::Not:    return "!";
            case Operator::Neg:    return "-";
        }
        return "unknown";
    }

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
        CastExpr,
        DerefExpr,
        ParenExpr,
        CallExpr, ArgList,
        IndexExpr,
        FieldAccess,
        BorrowExpr,
        NewExpr,
        StructInit, FieldInit, FieldInitList,

        LabeledExpr,
        BlockExpr,
        IfExpr,
        ForExpr, ForInExpr,
        MatchExpr, MatchArm,

        ExprStmt,
        LetStmt,
        ReturnStmt,
        BreakStmt,
        ContinueStmt,
        AssignStmt, 

        NamedType, DynType, RefType, TupleType, TypeArgList,
        FuncDecl, FuncSign,
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
            int64_t  i64;
            double   f64;
            uint32_t child;

            struct { uint32_t start, count; }                   list;
            struct { uint32_t lhs; Operator op; uint32_t rhs; } binary;
            struct { Operator op; uint32_t operand; }           unary;
            struct { uint32_t expr, type; }                     cast;
            struct { uint32_t cond, then_branch, else_branch; } if_expr;

            struct { uint32_t first, second; }                  pair;
            struct { uint32_t target, member; }                 access;
            struct { uint32_t callee, args; }                   call;
            struct { uint32_t label, body; }                    labeled;
            struct { uint32_t ident, type; uint8_t is_mut; }    typed_ident;
            struct { uint32_t operand; uint8_t is_mut; }        borrow;
            struct { uint32_t ident, args; }                    named_type;
            struct { uint32_t inner; uint8_t is_mut; }          ref_type;

            struct { uint32_t name, params, ret_type; }         func_sign;
            struct { uint32_t sign, body; uint8_t is_pub; }     func_decl;
            struct { uint32_t name, fields; uint8_t is_pub; }   struct_decl;
            struct { uint32_t target, value; }                  let;

            struct { uint32_t pattern, guard, body; }           match_arm;

            struct { uint32_t type, args; uint8_t is_mut; }     new_expr;
            struct { uint32_t label, value; }                   break_stmt;
            struct { uint32_t label; }                          continue_stmt;

            struct { uint32_t cond, body; }                     for_expr;
            struct { uint32_t var, iterable, body; }            for_in;
        } data;
    };
}