#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include "Zenith/Compiler/CTType.h"

namespace Zenith {
    enum class Severity : uint8_t
    {
        Error,
        Warning,
        Note
    };

    enum class DiagType : uint8_t
    {
        // Declaration & Signature
        DuplicateFunction,
        MissingMainFunction,
        InvalidMainSignature,
        NestedFunction,
        MissingParamType,
        UnknownTypeName,

        // Scope & Symbol
        UnknownIdentifier,
        RedeclaredIdentifier,

        // Expression & Operation
        OperandTypeMismatch,
        InvalidUnaryOp,
        WrongArgCount,
        CannotInferType,
        CoerceFailure,

        // Type Mismatch
        LetTypeMismatch,
        ReturnTypeMismatch,
        ArgTypeMismatch,
        AssignTypeMismatch,
        BreakTypeMismatch,

        // Statement & Control Flow
        ImmutableAssignment,
        InvalidAssignTarget,
        UndeclaredLabel,
        InvalidContinue,
        BlockMissingBreak,

        // General
        UnsupportedFeature,
        TooManyErrors,
    };

    struct Diagnostic
    {
        std::string message;
        uint32_t node, token;            
        CTType expected = CTType::Error(), actual = CTType::Error();
        Severity severity = Severity::Error;
        DiagType diag;
    };

    struct Error
    {
        DiagType type;
        std::string_view message;
    }; 
}