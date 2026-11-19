#include "Zenith/Compiler/CTType.h"
#include "Zenith/Compiler/Constraint.h"
#include "Zenith/Compiler/Diagnostic.h"
#include "Zenith/Compiler/Utils.h"
#include "Zenith/Compiler/Node.h"
#include "Zenith/Compiler/TypeChecker.h"

#include <string>
#include <string_view>

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
            case NodeType::BlockExpr:
            {
                ACTUAL(CheckBlock(node, expected, "", true));
            }
            
            default: COMMIT_ERROR(DiagType::UnsupportedFeature, "unsupported expression");
        }

        if (actual == CTType::Error() || expected == CTType::Error()) 
            return SetNodeType(node, CTType::Error());

        
        if (expected != CTType::Unknown())
        {
            if (expected.IsVar() || actual.IsVar())
            {
                m_Solver.Emit({
                    .kind = Constraint::Kind::Eq,
                    .lhs = expected,
                    .rhs = actual,
                    .diag = {
                        .node = node,
                        .token = n.start_token,
                        .diag = diag
                    }
                });

                return SetNodeType(node, actual.IsVar() ? expected : actual);
            }
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


    CTType TypeChecker::CheckBlock(uint32_t node, CTType expected, std::string_view label, bool push_target)
    {
        if (node == ERROR_NODE || node >= m_Ast.nodes.size()) return CTType::Error();
        if (expected == CTType::Error()) return CTType::Error();

        CTType target_type = (expected != CTType::Unknown()) ? expected : m_Solver.Fresh();

        PushScope();
        if (push_target) PushTarget({ 
            .label = label,
            .expected = target_type,
            .is_loop = false,
            .has_break = false
        });

        const auto& n = m_Ast.nodes[node];
        uint32_t stmt_cnt = ListCount(node);
        for (size_t i = 0; i < stmt_cnt; ++i)
            CheckStmt(ListItem(node, i));   

        bool has_break = false;
        if (push_target) has_break = PopTarget().has_break;
        PopScope();

        if (push_target && !has_break)
        {
            if (target_type.IsVar())
                m_Solver.Emit({
                    .kind = Constraint::Kind::Eq,
                    .lhs = target_type,
                    .rhs = CTType::Unit(),
                    .diag = { 
                        .node = node, 
                        .token = m_Ast.nodes[node].start_token, 
                        .diag = DiagType::BreakTypeMismatch 
                    }
                });
            else if (target_type != CTType::Unit())
            {
                AddError(node, { DiagType::BlockMissingBreak, "Block does not break with a value of expected type" });
                return CTType::Error();
            }
        }

        return target_type;
    }

    void TypeChecker::CheckStmt(uint32_t node)
    {
        if (node == ERROR_NODE || node >= m_Ast.nodes.size()) return;
        const auto& n = m_Ast.nodes[node];

        switch (n.type)
        {
            case NodeType::LetStmt:
            {
                uint32_t target = n.data.let.target;
                uint32_t value = n.data.let.value;

                if (target == ERROR_NODE) { ValidateExpr(value, CTType::Unknown()); SetNodeType(node, CTType::Error()); break; }
                uint32_t ident = ERROR_NODE, type = ERROR_NODE; bool is_mut = false;

                const auto& target_node = m_Ast.nodes[target];
                if (target_node.type == NodeType::Identifier) ident = target;
                else if (target_node.type == NodeType::TypedIdentifier)
                {
                    auto ti = target_node.data.typed_ident;
                    ident = ti.ident; type = ti.type; is_mut = (ti.is_mut != 0);
                }
                else 
                {
                    AddError(target, { .type = DiagType::UnsupportedFeature, .message = "unsupported let binding target" });
                    ValidateExpr(value, CTType::Unknown());
                    SetNodeType(node, CTType::Error());
                    break;
                }

                std::string_view name = GetName(ident);
                CTType var_type = (type != ERROR_NODE) ? ResolveTypeNode(type) : CTType::Unknown();
                CTType val_type = ValidateExpr(value, var_type, DiagType::LetTypeMismatch);
                if (type == ERROR_NODE) var_type = val_type;
                SetNodeType(node, var_type);
                SetNodeType(ident, var_type);

                if (!AddSymbol({ name, m_CurrentDepth, var_type, is_mut }))
                {
                    std::string msg = "redeclared variable '" + std::string(name) + "'";
                    AddError(ident, { DiagType::RedeclaredIdentifier, msg });
                }
                break;
            }
            case NodeType::AssignStmt:
            {
                uint32_t target = n.data.assign.target;
                uint32_t value = n.data.assign.value;

                if (target == ERROR_NODE || target >= m_Ast.nodes.size())
                {
                    ValidateExpr(value, CTType::Unknown());
                    break;
                }

                if (m_Ast.nodes[target].type != NodeType::Identifier)
                {
                    // support general assignable expr
                    AddError(target, { DiagType::InvalidAssignTarget, "invalid assignment target" });
                    ValidateExpr(value, CTType::Unknown());
                    SetNodeType(node, CTType::Error());
                    break;
                }

                std::string_view name = GetName(target);
                auto symbol = FindSymbols(name);
                if (!symbol.has_value())
                {
                    std::string msg = "unknown identifier '" + std::string(name) + "'";
                    AddError(target, { .type = DiagType::UnknownIdentifier, .message = msg });
                    ValidateExpr(value, CTType::Unknown());
                    SetNodeType(target, CTType::Error());
                    SetNodeType(node, CTType::Error());
                    break;
                }

                if (!symbol->is_mut)
                {
                    std::string msg = "cannot assign twice to immutable variable '" + std::string(name) + "'";
                    AddError(target, { DiagType::ImmutableAssignment, msg });
                }

                ValidateExpr(value, symbol->type, DiagType::AssignTypeMismatch);
                SetNodeType(target, symbol->type);
                SetNodeType(node, CTType::Unit());
                break;
            }
            case NodeType::ReturnStmt:
            {
                m_CurrentFunc.has_ret = true;
                uint32_t expr = n.data.child;
                if (expr != ERROR_NODE)
                    ValidateExpr(expr, m_CurrentFunc.ret_type, DiagType::ReturnTypeMismatch);
                else if (m_CurrentFunc.ret_type != CTType::Unit() && m_CurrentFunc.ret_type != CTType::Error())
                {
                    std::string msg = Utils::FormatTypeMismatch(DiagType::ReturnTypeMismatch, m_CurrentFunc.ret_type, CTType::Unit());
                    AddError(node, { DiagType::ReturnTypeMismatch, msg });
                }
                SetNodeType(node, CTType::Unit());
                break;
            }
            case NodeType::BreakStmt:
            {
                uint32_t label = n.data.break_stmt.label;
                uint32_t value = n.data.break_stmt.value;

                ControlTarget* target = FindBreak(label);
                if (!target)
                {
                    if (label != ERROR_NODE)
                        AddError(label, { DiagType::UndeclaredLabel, "undeclared label '" + std::string(GetName(label)) + "'" });
                    else
                        AddError(node, { DiagType::UndeclaredLabel, "'break' outside of loop or block" });

                    if (value != ERROR_NODE) ValidateExpr(value, CTType::Unknown());
                    SetNodeType(node, CTType::Error());
                    break;
                }

                target->has_break = true;
                if (value != ERROR_NODE)
                    ValidateExpr(value, target->expected, DiagType::BreakTypeMismatch);
                else if (target->expected.IsVar())
                    m_Solver.Emit({
                        .kind = Constraint::Kind::Eq,
                        .lhs = target->expected,
                        .rhs = CTType::Unit(),
                        .diag = { .node = node, .token = n.start_token, .diag = DiagType::BreakTypeMismatch }
                    });
                else if (target->expected != CTType::Unit() && target->expected != CTType::Error())
                {
                    std::string msg = Utils::FormatTypeMismatch(DiagType::BreakTypeMismatch, target->expected, CTType::Unit());
                    AddError(node, { DiagType::BreakTypeMismatch, msg });
                }
                SetNodeType(node, CTType::Unit());
                break;
            }
            case NodeType::ContinueStmt:
            {
                uint32_t label = n.data.continue_stmt.label;
                ControlTarget* target = FindContinue(label);
                if (!target)
                {
                    if (label != ERROR_NODE && !FindBreak(label))
                        AddError(label, { DiagType::UndeclaredLabel, "undeclared label '" + std::string(GetName(label)) + "'" });
                    else
                        AddError(node, { DiagType::InvalidContinue, "'continue' must target an enclosing loop" });
                    SetNodeType(node, CTType::Error());
                    break;
                }
                SetNodeType(node, CTType::Unit());
                break;
            }
            case NodeType::ExprStmt:
            {
                ValidateExpr(n.data.child, CTType::Unknown());
                SetNodeType(node, CTType::Unit());
                break;
            }
            // TODO: add implementation
            case NodeType::FuncDecl:
            {
                AddError(node, { DiagType::NestedFunction, "nested functions are not supported" });
                break;
            }
            default:
            {
                AddError(node, { DiagType::UnsupportedFeature, "unsupported statement" });
                break;
            }
        }
    }
}