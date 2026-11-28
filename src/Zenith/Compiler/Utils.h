#pragma once

#include "Zenith/Compiler/Ast.h"
#include "Zenith/Compiler/CTType.h"
#include "Zenith/Compiler/Node.h"
#include "Zenith/VM/Value.h"
#include "Zenith/VM/Opcode.h"

#include <optional>
#include <cmath>
#include <cstdint>
#include <limits>
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
                case CTType::Kind::I8:      return "i8";
                case CTType::Kind::I16:     return "i16";
                case CTType::Kind::I32:     return "i32";
                case CTType::Kind::I64:     return "i64";
                case CTType::Kind::U8:      return "u8";
                case CTType::Kind::U16:     return "u16";
                case CTType::Kind::U32:     return "u32";
                case CTType::Kind::U64:     return "u64";
                case CTType::Kind::F32:     return "f32";
                case CTType::Kind::F64:     return "f64";
                default:                    return "<unknown>";
            }
        }

        constexpr Type ToVMType(CTType type) noexcept
        {
            switch (type.kind)
            {
                case CTType::Kind::I8:
                case CTType::Kind::I16:
                case CTType::Kind::I32:  return Type::I32;
                case CTType::Kind::U8:
                case CTType::Kind::U16:
                case CTType::Kind::U32:  return Type::U32;
                case CTType::Kind::I64:  return Type::I64;
                case CTType::Kind::U64:
                case CTType::Kind::Bool: return Type::U64;
                case CTType::Kind::F32:  return Type::F32;
                case CTType::Kind::F64:  return Type::F64;
                default:                 return Type::None;
            }
        }


        constexpr bool IsSignedInteger(CTType type) noexcept
        {
            switch (type.kind)
            {
                case CTType::Kind::I8:
                case CTType::Kind::I16:
                case CTType::Kind::I32:
                case CTType::Kind::I64:
                    return true;
                default:
                    return false;
            }
        }

        constexpr bool IsUnsignedInteger(CTType type) noexcept
        {
            switch (type.kind)
            {
                case CTType::Kind::U8:
                case CTType::Kind::U16:
                case CTType::Kind::U32:
                case CTType::Kind::U64:
                    return true;
                default:
                    return false;
            }
        }

        constexpr bool IsInteger(CTType type) noexcept
        {
            return IsSignedInteger(type) || IsUnsignedInteger(type);
        }

        constexpr bool IsFloat(CTType type) noexcept
        {
            return type == CTType::F32() || type == CTType::F64();
        }

        constexpr bool IsNumeric(CTType type) noexcept
        {
            return IsInteger(type) || IsFloat(type);
        }

        constexpr bool IsSignedNumeric(CTType type) noexcept
        {
            return IsSignedInteger(type) || IsFloat(type);
        }

        constexpr bool ValidatePrimitiveCast(CTType src, CTType dst) noexcept
        {
            if (src == dst) return true;
            if (IsNumeric(src) && IsNumeric(dst)) return true;
            if (src == CTType::Bool() && IsInteger(dst)) return true;
            if (IsInteger(src) && dst == CTType::Bool()) return true;
            return false;
        }

        constexpr bool CheckIntRange(int64_t val, CTType target) noexcept
        {
            switch (target.kind)
            {
                case CTType::Kind::I8:
                    return val >= std::numeric_limits<int8_t>::min() && val <= std::numeric_limits<int8_t>::max();
                case CTType::Kind::I16:
                    return val >= std::numeric_limits<int16_t>::min() && val <= std::numeric_limits<int16_t>::max();
                case CTType::Kind::I32:
                    return val >= std::numeric_limits<int32_t>::min() && val <= std::numeric_limits<int32_t>::max();
                case CTType::Kind::I64:
                    return true;
                case CTType::Kind::U8:
                    return val >= 0 && static_cast<uint64_t>(val) <= std::numeric_limits<uint8_t>::max();
                case CTType::Kind::U16:
                    return val >= 0 && static_cast<uint64_t>(val) <= std::numeric_limits<uint16_t>::max();
                case CTType::Kind::U32:
                    return val >= 0 && static_cast<uint64_t>(val) <= std::numeric_limits<uint32_t>::max();
                case CTType::Kind::U64:
                    return val >= 0;
                default:
                    return false;
            }
        }

        inline bool CheckFloatRange(double val, CTType target) noexcept
        {
            switch (target.kind)
            {
                case CTType::Kind::F32:
                    return !std::isfinite(val) || std::abs(val) <= static_cast<double>(std::numeric_limits<float>::max());
                case CTType::Kind::F64:
                    return true;
                default:
                    return false;
            }
        }

        constexpr CTType CheckUnaryOp(Operator op, CTType operand) noexcept
        {
            switch (op)
            {
                case Operator::Not:
                    return (operand == CTType::Bool()) ? CTType::Bool() : CTType::Error();
                case Operator::Neg:
                    return IsSignedNumeric(operand) ? operand : CTType::Error();
                default:
                    return CTType::Error();
            }
        }

        constexpr CTType CheckBinaryOp(Operator op, CTType lhs, CTType rhs) noexcept
        {
            if (lhs != rhs) return CTType::Error();

            switch (op)
            {
                case Operator::Add:
                case Operator::Sub:
                case Operator::Mul:
                case Operator::Div:
                    return IsNumeric(lhs) ? lhs : CTType::Error();
                case Operator::Mod:
                    return IsInteger(lhs) ? lhs : CTType::Error();
                case Operator::Eq:
                case Operator::Ne:
                    return (IsNumeric(lhs) || lhs == CTType::Bool()) ? CTType::Bool() : CTType::Error();
                case Operator::Lt:
                case Operator::Lte:
                case Operator::Gt:
                case Operator::Gte:
                    return IsNumeric(lhs) ? CTType::Bool() : CTType::Error();
                case Operator::And:
                case Operator::Or:
                    return (lhs == CTType::Bool()) ? CTType::Bool() : CTType::Error();
                default:
                    return CTType::Error();
            }
        }

        constexpr bool IsArithmeticOp(Operator op) noexcept
        {
            switch (op)
            {
                case Operator::Add:
                case Operator::Sub:
                case Operator::Mul:
                case Operator::Div:
                case Operator::Mod:
                    return true;
                default:
                    return false;
            }
        }

        constexpr std::optional<CastKind> FindNumericCast(Type src, Type dst) noexcept
        {
            if (src == dst) return std::nullopt;

            if (src == Type::I32 && (dst == Type::I64 || dst == Type::U64)) return CastKind::I32ToI64;
            if ((src == Type::I64 || src == Type::U64) && (dst == Type::I32 || dst == Type::U32)) return CastKind::I64ToI32;

            if (src == Type::I32) return (dst == Type::F32) ? std::optional(CastKind::I32ToF32) : (dst == Type::F64 ? std::optional(CastKind::I32ToF64) : std::nullopt);
            if (src == Type::U32) return (dst == Type::F32) ? std::optional(CastKind::U32ToF32) : (dst == Type::F64 ? std::optional(CastKind::U32ToF64) : std::nullopt);
            if (src == Type::I64) return (dst == Type::F32) ? std::optional(CastKind::I64ToF32) : (dst == Type::F64 ? std::optional(CastKind::I64ToF64) : std::nullopt);
            if (src == Type::U64) return (dst == Type::F32) ? std::optional(CastKind::U64ToF32) : (dst == Type::F64 ? std::optional(CastKind::U64ToF64) : std::nullopt);

            if (src == Type::F32)
            {
                switch (dst)
                {
                    case Type::I32: return CastKind::F32ToI32;
                    case Type::U32: return CastKind::F32ToU32;
                    case Type::I64: return CastKind::F32ToI64;
                    case Type::U64: return CastKind::F32ToU64;
                    case Type::F64: return CastKind::F32ToF64;
                    default:        return std::nullopt;
                }
            }
            if (src == Type::F64)
            {
                switch (dst)
                {
                    case Type::I32: return CastKind::F64ToI32;
                    case Type::U32: return CastKind::F64ToU32;
                    case Type::I64: return CastKind::F64ToI64;
                    case Type::U64: return CastKind::F64ToU64;
                    case Type::F32: return CastKind::F64ToF32;
                    default:        return std::nullopt;
                }
            }
            return std::nullopt;
        }


        constexpr bool GetName(const AstView& ast, uint32_t node, std::string_view& out) noexcept
        {
            if (node >= ast.nodes.size()) return false;
            const Node& n = ast.nodes[node];
            if (n.type != NodeType::Identifier && n.type != NodeType::Label) return false;
            if (n.start_token >= ast.tokens.size()) return false;
            out = ast.tokens[n.start_token].literal;
            return true;
        }

        constexpr bool ListCount(const AstView& ast, uint32_t list_node, uint32_t& out) noexcept
        {
            if (list_node >= ast.nodes.size()) return false;
            out = ast.nodes[list_node].data.list.count;
            return true;
        }

        constexpr bool ListItem(const AstView& ast, uint32_t list_node, uint32_t i, uint32_t& out) noexcept
        {
            if (list_node >= ast.nodes.size()) return false;
            size_t idx = static_cast<size_t>(ast.nodes[list_node].data.list.start) + i;
            if (idx >= ast.extra.size()) return false;
            out = ast.extra[idx];
            return true;
        }
    }
}
