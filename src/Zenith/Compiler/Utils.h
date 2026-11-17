#pragma once

#include "Zenith/Compiler/CTType.h"
#include "Zenith/Compiler/Diagnostic.h"
#include "Zenith/Compiler/Node.h"

#include <string>
#include <string_view>

namespace Zenith {
    namespace Utils {
        constexpr std::string_view TypeToString(CTType type) noexcept
        {
            switch (type.kind)
            {
                case CTType::Kind::Error:   return "<error>";
                case CTType::Kind::Unknown: return "<unknown>";
                case CTType::Kind::Var:     return "<var>";
                case CTType::Kind::Unit:    return "unit";
                case CTType::Kind::Bool:    return "bool";
                case CTType::Kind::I64:     return "i64";
                case CTType::Kind::F64:     return "f64";
                default:              return "<unknown>";
            }
        }

        inline std::string FormatTypeMismatch(DiagType diag, CTType expected, CTType actual)
        {
            std::string_view prefix = "Type mismatch";
            switch (diag)
            {
                case DiagType::LetTypeMismatch:    prefix = "Let binding type mismatch"; break;
                case DiagType::ReturnTypeMismatch: prefix = "Return type mismatch"; break;
                case DiagType::ArgTypeMismatch:    prefix = "Argument type mismatch"; break;
                case DiagType::AssignTypeMismatch: prefix = "Assignment type mismatch"; break;
                case DiagType::BreakTypeMismatch:  prefix = "Break value type mismatch"; break;
                default: break;
            }

            return std::string(prefix)
                + ": expected '" + std::string(TypeToString(expected))
                + "', got '" + std::string(TypeToString(actual)) + "'";
        }

        constexpr bool IsInteger(CTType type) noexcept
        {
            return type == CTType::I64();
        }

        constexpr bool IsFloat(CTType type) noexcept
        {
            return type == CTType::F64();
        }

        constexpr bool IsNumeric(CTType type) noexcept
        {
            return IsInteger(type) || IsFloat(type);
        }

        bool ValidatePrimitiveCast(CTType src, CTType dst) 
        {
            if (src == dst) return true;

            switch (src.kind) 
            {
                case CTType::Kind::I64:
                    return dst == CTType::F64() || dst == CTType::Bool();
                case CTType::Kind::F64:
                    return dst == CTType::I64();
                case CTType::Kind::Bool:
                    return dst == CTType::I64();
                default:
                    return false;
            }
        }

        CTType ValidateNumericLiteral(const Node& node, CTType expected)
        {
            if (node.type == NodeType::Integer)
            {
                int64_t val = node.data.i64;
                if (expected == CTType::Unknown() || expected == CTType::I64()) return CTType::I64();
                switch (expected.kind)
                {
                    /*
                    case CTType::I32:
                        if (val >= std::numeric_limits<int32_t>::min() && val <= std::numeric_limits<int32_t>::max())
                            return CTType::I32;
                        AddError(node, {DiagType::OperandTypeMismatch, "Integer literal out of range for 'i32'"});
                        return CTType::Error;

                    case CTType::U32:
                        if (val >= 0 && static_cast<uint64_t>(val) <= std::numeric_limits<uint32_t>::max())
                            return CTType::U32;
                        AddError(node, {DiagType::OperandTypeMismatch, "Integer literal out of range for 'u32'"});
                        return CTType::Error;

                    case CTType::I16:
                        if (val >= std::numeric_limits<int16_t>::min() && val <= std::numeric_limits<int16_t>::max())
                            return CTType::I16;
                        AddError(node, {DiagType::OperandTypeMismatch, "Integer literal out of range for 'i16'"});
                        return CTType::Error;

                    case CTType::U8:
                        if (val >= 0 && val <= std::numeric_limits<uint8_t>::max())
                            return CTType::U8;
                        AddError(node, {DiagType::OperandTypeMismatch, "Integer literal out of range for 'u8'"});
                        return CTType::Error;
                    */
                    default: return CTType::I64();
                }
            }

            if (node.type == NodeType::Decimal)
            {
                double val = node.data.f64;

                if (expected == CTType::Unknown() || expected == CTType::F64())
                    return CTType::F64();

                switch (expected.kind)
                {
                    /*
                    case CTType::F32:
                        if (std::isfinite(val) && (std::abs(val) <= std::numeric_limits<float>::max() || val == 0.0))
                            return CTType::F32;
                        AddError(node, {DiagType::OperandTypeMismatch, "Decimal literal out of range for 'f32'"});
                        return CTType::Error;
                    */
                    default: return CTType::F64();
                }
            }

            return CTType::Error();
        }
    }
}