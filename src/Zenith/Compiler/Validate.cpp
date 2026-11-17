#include "Zenith/Compiler/Utils.h"
#include "Zenith/Compiler/Node.h"
#include "Zenith/Compiler/TypeChecker.h"

#include <string>

#define ACTUAL(type) { actual = (type); break; }
#define ERROR() { ACTUAL(CTType::Error()); }
#define COMMIT_ERROR(diag, message) { AddError(node, { diag, message }); ERROR(); }

namespace Zenith {

    CTType TypeChecker::ValidateExpr(uint32_t node, CTType expected, DiagType diag)
    {
        if (node == ERROR_NODE || node >= m_Ast.nodes.size()) return CTType::Error();
        const auto& n = m_Ast.nodes[node];
        CTType actual = CTType::Error();
        switch (n.type)
        {
            case NodeType::Integer:
            case NodeType::Decimal: ACTUAL(Utils::ValidateNumericLiteral(n, expected));
            case NodeType::Bool: ACTUAL(CTType::Bool());
            case NodeType::ParenExpr: ACTUAL(ValidateExpr(n.data.child, expected, diag));
            case NodeType::Identifier:
            {
                auto name = GetName(node);
                auto symbol = FindSymbols(name);
                if (symbol.has_value()) ACTUAL(symbol->type);
                COMMIT_ERROR(DiagType::UnknownIdentifier, 
                    "unknown identifier '" + std::string(name) + "'");
            }
            case NodeType::Tuple:
            {
                // TODO: Update tuple support later
                COMMIT_ERROR(DiagType::UnsupportedFeature, 
                    "tuple expressions not supported yet");
            }
            case NodeType::UnaryExpr:
            {
                auto operand = ValidateExpr(n.data.unary.operand, CTType::Unknown());
                if (operand == CTType::Error()) ERROR();

                if (n.data.unary.op == Operator::Not)
                {
                    if (operand != CTType::Bool())
                        COMMIT_ERROR(DiagType::OperandTypeMismatch, 
                            "Unary '!' requires boolean operand");
                    ACTUAL(CTType::Bool());
                }
                if (n.data.unary.op == Operator::Neg)
                {
                    if (!Utils::IsNumeric(operand))
                        COMMIT_ERROR(DiagType::OperandTypeMismatch, 
                                "Unary '-' requires numeric operand");
                    ACTUAL(operand);
                }
                COMMIT_ERROR(DiagType::InvalidUnaryOp, 
                        "Unsupported unary operator");
            }
            case NodeType::BinaryExpr:
            {
                auto lhs = ValidateExpr(n.data.binary.lhs, CTType::Unknown());
                CTType rhs_expected = (lhs.kind == CTType::Kind::Error || 
                                        lhs.kind == CTType::Kind::Unknown) 
                    ? CTType::Unknown() : lhs;
                auto rhs = ValidateExpr(n.data.binary.rhs, rhs_expected);
                ACTUAL(ResolveBinaryOp(node, n.data.binary.op, lhs, rhs));
            }
            case NodeType::CastExpr:
            {
                auto src = ValidateExpr(n.data.cast.expr, CTType::Unknown());
                auto dst = ResolveTypeNode(n.data.cast.type);
                if (src == CTType::Error() || dst == CTType::Error()) ERROR();

                if(Utils::ValidatePrimitiveCast(src, dst)) ACTUAL(dst);

                COMMIT_ERROR(DiagType::CoerceFailure, "Invalid type cast");
            }
            case NodeType::CallExpr:
            {
                if (n.data.call.callee == ERROR_NODE || n.data.call.callee >= m_Ast.nodes.size()) ERROR();
                // TODO: support general callable expr
                auto callee = m_Ast.nodes[n.data.call.callee].type; 
                if (callee != NodeType::Identifier)
                    COMMIT_ERROR(DiagType::UnsupportedFeature, "Only direct function calls are supported");

                auto name = GetName(n.data.call.callee);
                auto sig = m_Table.Find(name);
                if (!sig.has_value()) 
                    COMMIT_ERROR(DiagType::UnknownIdentifier, "Cannot find out the function " + std::string(name))

                if (n.data.call.args == ERROR_NODE || n.data.call.args >= m_Ast.nodes.size()) ERROR();
                uint32_t count = ListCount(n.data.call.args);
                if (count != sig->params.size()) 
                    COMMIT_ERROR(DiagType::WrongArgCount, "Arg count mismatch");

                bool has_error = false;
                for (size_t i = 0; i < count; ++i)
                {
                    uint32_t arg = ListItem(n.data.call.args, i);
                    if (ValidateExpr(arg, sig->params[i], 
                                DiagType::ArgTypeMismatch) == CTType::Error()) has_error = true;
                }
                if (has_error) ERROR();
                ACTUAL(sig->ret_type);
            }
            
            default: COMMIT_ERROR(DiagType::UnsupportedFeature, "unsupported expression");
        }

        if (actual == CTType::Error()) return SetNodeType(node, actual);
        if (expected != CTType::Unknown())
        {
            if (actual != expected)
            {
                std::string msg = Utils::FormatTypeMismatch(diag, expected, actual);
                AddError(node, { diag, msg });
                return SetNodeType(node, CTType::Error());
            }
            return SetNodeType(node, expected);
        }

        return SetNodeType(node, actual);
    }
}