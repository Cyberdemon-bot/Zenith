#include "Zenith/Core/Assert.h"
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

    bool TypeChecker::Validate()
    {
        ZN_CORE_ASSERT(!m_Validated, "Validate() called twice");
        m_Validated = true;
        m_Errors.clear();

        m_NodeTypes.assign(m_Ast.nodes.size(), CTType::Error());
        CollectSignature();

        uint32_t child_size = ListCount(m_Ast.root);
        for (size_t i = 0; i < child_size; ++i)
        {
            const auto& decl = m_Ast.nodes[ListItem(m_Ast.root, i)];
            if (decl.type != NodeType::FuncDecl) continue;

            m_Solver.Reset();
            m_Targets.clear();

            const auto& sign = m_Ast.nodes[decl.data.func_decl.sign];

            auto func = m_Table.Find(GetName(sign.data.func_sign.name));
            if (!func.has_value()) continue;
            m_CurrentFunc = { .ret_type = func->ret_type, .has_ret = false };

            PushScope();

            uint32_t params_cnt = ListCount(sign.data.func_sign.params);
            for (uint32_t j = 0; j < params_cnt; ++j)
            {
                const uint32_t param_idx = ListItem(sign.data.func_sign.params, j);
                const auto& ti = m_Ast.nodes[param_idx].data.typed_ident;
                std::string_view param_name = GetName(ti.ident);
                CTType param_type = func->params[j];

                if (!AddSymbol({
                    .name = param_name,
                    .depth = m_CurrentDepth,
                    .type = param_type,
                    .is_mut = (ti.is_mut != 0)
                }))
                {
                    std::string msg = "redeclared parameter '" + std::string(param_name) + "'";
                    AddError(ti.ident, { DiagType::RedeclaredIdentifier, msg });
                }

                SetNodeType(param_idx, param_type);
                SetNodeType(ti.ident, param_type);
            }

            CheckBlock(decl.data.func_decl.body, CTType::Unit(), "", false);
            PopScope();

            std::string_view func_name = GetName(sign.data.func_sign.name);
            if (m_CurrentFunc.ret_type != CTType::Unit() &&
                m_CurrentFunc.ret_type != CTType::Error() &&
                !m_CurrentFunc.has_ret)
            {
                std::string msg = "function '" + std::string(func_name) + "' must return a value";
                AddError(sign.data.func_sign.name, { DiagType::ReturnTypeMismatch, msg });
            }

            m_Solver.Solve([this](Diagnostic&& d) {
                m_Errors.push_back(std::move(d));
            });

            for (uint32_t idx = 1; idx < m_NodeTypes.size(); ++idx)
            {
                if (!m_NodeTypes[idx].IsVar()) continue;
                CTType resolved = m_Solver.Resolve(m_NodeTypes[idx]);
                if (resolved.IsVar())
                {
                    AddError(idx, { DiagType::CannotInferType, "cannot infer type" });
                    m_NodeTypes[idx] = CTType::Error();
                }
                else m_NodeTypes[idx] = resolved;
            }
        }

        for (const auto& d : m_Errors) 
            if (d.severity == Severity::Error) return false;
        return true;
    }

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
            case NodeType::BlockExpr: ACTUAL(CheckBlock(node, expected, "", true));
            case NodeType::IfExpr: ACTUAL(CheckIf(node, expected)); 
            case NodeType::ForExpr: ACTUAL(CheckFor(node, expected));
            case NodeType::LabeledExpr:
            {
                uint32_t label = n.data.labeled.label;
                uint32_t body  = n.data.labeled.body;
                if (label == ERROR_NODE || body == ERROR_NODE || body >= m_Ast.nodes.size()) ERROR();

                std::string_view name = GetName(label);
                const auto& expr = m_Ast.nodes[body];
                switch (expr.type)
                {
                    case NodeType::BlockExpr: ACTUAL(CheckBlock(body, expected, name, true));
                    case NodeType::IfExpr:    ACTUAL(CheckIf(body, expected, name));
                    case NodeType::ForExpr:   ACTUAL(CheckFor(body, expected, name));
                    default: COMMIT_ERROR(DiagType::UnsupportedFeature, "unsupported expression in labeled expression");
                }
                break;
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

    CTType TypeChecker::CheckIf(uint32_t node, CTType expected, std::string_view label)
    {
        if (node == ERROR_NODE || node >= m_Ast.nodes.size()) return CTType::Error();
        if (expected == CTType::Error()) return CTType::Error();

        const auto& ie = m_Ast.nodes[node].data.if_expr; 
        ValidateExpr(ie.cond, CTType::Bool(), DiagType::OperandTypeMismatch);

        bool has_else = (ie.else_branch != ERROR_NODE);
        bool is_check = (expected != CTType::Unknown());
        CTType res_type = has_else ? (is_check ? expected : m_Solver.Fresh()) : CTType::Unit();

        if (!label.empty())
            PushTarget({
                .label = label,
                .expected = res_type,
                .is_loop = false, .has_break = false
            });

        CTType then_type = ValidateExpr(ie.then_branch, res_type);
        CTType else_type = has_else ? ValidateExpr(ie.else_branch, res_type) : CTType::Unit();

        if (!label.empty()) PopTarget();
        if (then_type == CTType::Error() || else_type == CTType::Error()) return CTType::Error();

        return res_type;
    }

    CTType TypeChecker::CheckFor(uint32_t node, CTType expected, std::string_view label)
    {
        if (node == ERROR_NODE || node >= m_Ast.nodes.size()) return CTType::Error();
        if (expected == CTType::Error()) return CTType::Error();

        const auto& fe = m_Ast.nodes[node].data.for_expr; 
        if (fe.cond != ERROR_NODE)
            ValidateExpr(fe.cond, CTType::Bool(), DiagType::OperandTypeMismatch);

        CTType target_type = (expected != CTType::Unknown()) ? expected : m_Solver.Fresh();
        PushTarget({ 
            .label = label, 
            .expected = target_type, 
            .is_loop = true, 
            .has_break = false 
        });

        CheckBlock(fe.body, CTType::Unit(), "", false);
        bool has_break = PopTarget().has_break;

        if (!has_break)
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
                AddError(node, { DiagType::BlockMissingBreak, "Loop does not break with a value of expected type" });
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