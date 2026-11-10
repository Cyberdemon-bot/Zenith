#include "Zenith/Compiler/Compiler.h"
#include "Zenith/Compiler/Node.h"
#include "Zenith/Compiler/TypeChecker.h"

namespace Zenith {

    CTType TypeChecker::InferExpr(uint32_t node)
    {
        if (node == ERROR_NODE || node >= m_Ast.nodes.size()) return CTType::Error;
        const auto& n = m_Ast.nodes[node];
        auto type = n.type;
        switch (type) 
        {
            case NodeType::Integer: return CTType::I64;
            case NodeType::Decimal: return CTType::F64;
            case NodeType::Bool: return CTType::Bool;
            case NodeType::Identifier:
            {
                auto name = GetName(node);
                auto symbol = FindSymbols(name);
                if (symbol.has_value()) return symbol.value().type;
                AddError(node, {DiagType::UnknownIdentifier,
                        "unknown identifier '" + std::string(name) + "'"});
                return CTType::Error;
            }
            case NodeType::Tuple:
            {
                // TODO: Update tuple support later
                AddError(node, {DiagType::UnsupportedFeature, 
                        "tuple expressions not supported yet"});
                return CTType::Error;
            }
            case NodeType::ParenExpr:
            {
                if (n.data.child == ERROR_NODE || n.data.child >= m_Ast.nodes.size()) return CTType::Error;
                return InferExpr(n.data.child);
            }
            case NodeType::UnaryExpr:
            {
                auto operand = InferExpr(n.data.unary.operand);
                if (operand == CTType::Error) return CTType::Error;

                if (n.data.unary.op == Operator::Not)
                {
                    if (operand != CTType::Bool)
                    {
                        AddError(node, {DiagType::OperandTypeMismatch, 
                                "Unary '!' requires boolean operand"});
                        return CTType::Error;
                    }
                    return CTType::Bool;
                }
                if (n.data.unary.op == Operator::Neg)
                {
                    if (operand != CTType::I64 && operand != CTType::F64)
                    {
                        AddError(node, {DiagType::OperandTypeMismatch, 
                                "Unary '-' requires numeric operand"});
                        return CTType::Error;
                    }
                    return operand;
                }
                AddError(node, {DiagType::InvalidUnaryOp, 
                        "Unsupported unary operator"});
                return CTType::Error;
            }
            case NodeType::CastExpr:
            {
                auto src = InferExpr(n.data.cast.expr);
                auto dst = ResolveTypeNode(n.data.cast.type);
                if (src == CTType::Error || dst == CTType::Error) return CTType::Error;

                if (src == dst) return dst;
                if ((src == CTType::I64 && dst == CTType::F64) ||
                    (src == CTType::F64 && dst == CTType::I64) ||
                    (src == CTType::Bool && dst == CTType::I64) ||
                    (src == CTType::I64 && dst == CTType::Bool))
                {
                    return dst;
                }

                AddError(node, {DiagType::CoerceFailure, "Invalid type cast"});
                return CTType::Error;
            }
            case NodeType::CallExpr:
            {
                if (n.data.call.callee == ERROR_NODE || n.data.call.callee >= m_Ast.nodes.size()) return CTType::Error;

                // TODO: support general callable expr
                if (m_Ast.nodes[n.data.call.callee].type != NodeType::Identifier)
                {
                    AddError(node, {DiagType::UnsupportedFeature, "Only direct function calls are supported"});
                    return CTType::Error;
                }

                auto name = GetName(n.data.call.callee);
                auto sig = m_Table.Find(name);
                if (!sig.has_value()) 
                {
                    AddError(node, {DiagType::UnknownIdentifier, "Cannot find out the function " + std::string(name)});
                    return CTType::Error;
                }

                if (n.data.call.args == ERROR_NODE || n.data.call.args >= m_Ast.nodes.size()) return CTType::Error;
                auto arg_list = m_Ast.nodes[n.data.call.args];
                uint32_t count = arg_list.data.list.count;
                uint32_t start = arg_list.data.list.start;

                if (count != sig->params.size())
                {
                    AddError(node, {DiagType::WrongArgCount, "Arg count mismatch"});
                    return CTType::Error;
                }

                for (size_t i = 0; i < count; ++i)
                {
                    if (start + i >= m_Ast.extra.size()) return CTType::Error;
                    uint32_t arg_idx = m_Ast.extra[start + i];
                    if (!CheckExpr(arg_idx, sig->params[i], {DiagType::ArgTypeMismatch, 
                                    "Argument's type in the function call must match with the declaration!"})) 
                        return CTType::Error;
                }

                return sig->ret_type;
            }
            case NodeType::BinaryExpr:
            {
                auto lhs = InferExpr(n.data.binary.lhs);
                auto rhs = InferExpr(n.data.binary.rhs);
                auto op  = n.data.binary.op;

                if (lhs == CTType::Error || rhs == CTType::Error) return CTType::Error;
                if (lhs != rhs)
                {
                    AddError(node, {DiagType::OperandTypeMismatch, 
                            "Operands of binary operator must have the same type"});
                    return CTType::Error;
                }

                switch (op)
                {
                    case Operator::Add:
                    case Operator::Sub:
                    case Operator::Mul:
                    case Operator::Div:
                    {
                        if (lhs != CTType::I64 && lhs != CTType::F64)
                        {
                            AddError(node, {DiagType::OperandTypeMismatch, 
                                    "Arithmetic operators require numeric operands"});
                            return CTType::Error;
                        }
                        return lhs;
                    }
                    case Operator::Mod:
                    {
                        if (lhs != CTType::I64)
                        {
                            AddError(node, {DiagType::OperandTypeMismatch, 
                                    "Operator '%' requires integer operands"});
                            return CTType::Error;
                        }
                        return CTType::I64;
                    }
                    case Operator::Eq:
                    case Operator::Ne: return CTType::Bool;
                    case Operator::Lt:
                    case Operator::Lte:
                    case Operator::Gt:
                    case Operator::Gte:
                    {
                        if (lhs != CTType::I64 && lhs != CTType::F64)
                        {
                            AddError(node, {DiagType::OperandTypeMismatch, 
                                    "Ordered comparison requires numeric operands"});
                            return CTType::Error;
                        }
                        return CTType::Bool;
                    }
                    case Operator::And:
                    case Operator::Or:
                    {
                        if (lhs != CTType::Bool) 
                        {
                            AddError(node, {DiagType::OperandTypeMismatch, 
                                    "Logical operators require boolean operands"});
                            return CTType::Error;
                        }
                        return CTType::Bool;
                    }
                    default:
                    {
                        AddError(node, {DiagType::OperandTypeMismatch, 
                                "Unsupported binary operator"});
                        return CTType::Error;
                    }
                }
            }
            default: return CTType::Error;
        }
    }
}