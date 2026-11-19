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
        InvalidBinaryOp,
        WrongArgCount,
        CannotInferType,
        CoerceFailure,
        LiteralOutOfRange,

        // Type Mismatch
        LetTypeMismatch,
        ReturnTypeMismatch,
        ArgTypeMismatch,
        AssignTypeMismatch,
        BreakTypeMismatch,
        MissingReturn,

        // Statement & Control Flow
        ImmutableAssignment,
        InvalidAssignTarget,
        UndeclaredLabel,
        InvalidContinue,
        BreakOutsideLoop,
        BlockMissingBreak,

        // General
        UnsupportedFeature,
        TooManyErrors,
    };

    struct Diagnostic
    {
        std::string args;
        uint32_t node = 0, token = 0;
        CTType expected = CTType::Error(), actual = CTType::Error();
        Severity severity = Severity::Error;
        DiagType diag = DiagType::UnsupportedFeature;
    };

    struct Error
    {
        DiagType type;
        std::string_view args = {};
        CTType expected = CTType::Error();
        CTType actual = CTType::Error();
    };

    std::string FormatDiagnostic(const Diagnostic& diag);
}
