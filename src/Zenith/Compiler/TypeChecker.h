#pragma once

#include "Zenith/Compiler/Compiler.h"
#include "Zenith/Compiler/Ast.h"

#include <vector>
#include <unordered_map>

namespace Zenith {

    enum class Severity : uint8_t 
    { 
        Error,
        Warning, 
        Note 
    };

    enum class DiagType : uint8_t 
    {
        DuplicateFunction,
        UnknownTypeName,
        NestedFunction,              

       
        UnknownIdentifier,
        OperandTypeMismatch,         // 1 + true
        InvalidUnaryOp,              // -true
        WrongArgCount,               // func add(a, b); add(1)
        CannotInferType,             // let x = <sth that cannot be infered>

        LetTypeMismatch,             // let x: i64 = 1.0
        ReturnTypeMismatch,          // return 1.0 in func -> i64
        ArgTypeMismatch,             // f(1.0) but f(a: i64)
        AssignTypeMismatch,          // x: i64 = 1.0
        CoerceFailure,               // cannot coerce

        TooManyErrors,               // stop after n errors
    };

    struct Diagnostic
    {
        Severity severity = Severity::Error;
        DiagType diag;
        std::string message;
        uint32_t node, token;
        CTType expected = CTType::Error, actual = CTType::Error;
    };

    struct FuncSign
    {
        // [param1, param2, ..., param<count> / ret1, ret2, ..., ret<count>]
        uint32_t params_count;
        uint32_t rets_count; 
        uint16_t proto_id;
    };

    class SignatureTable
    {
    public:
        void Freeze();
    private:
        std::vector<CTType> m_TPool;
        std::vector<FuncSign> m_Signs;
        std::unordered_map<std::string_view, uint32_t> m_FuncIndex;

        bool m_Frozen = false;
    };

    class TypeChecker
    {
    public:
        TypeChecker(AstView ast)
            : m_Ast(ast)
        {}

        bool Validate();
    private:
        AstView m_Ast;
    };
}