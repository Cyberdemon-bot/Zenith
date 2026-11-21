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
#define COMMIT_ERROR(...) { AddError(node, { __VA_ARGS__ }); ERROR(); }

namespace Zenith {

    bool TypeChecker::Validate()
    {
        ZN_CORE_ASSERT(!m_Validated, "Validate() called twice");
        m_Validated = true;
        m_Errors.clear();

        m_NodeTypes.assign(m_Ast.nodes.size(), CTType::Unknown());
        CollectSignature();

        uint32_t child_size = ListCount(m_Ast.root);
        for (uint32_t i = 0; i < child_size; ++i)
        {
            const uint32_t decl_idx = ListItem(m_Ast.root, i);
            const auto& decl = m_Ast.nodes[decl_idx];
            if (decl.type != NodeType::FuncDecl) continue;
            if (m_NodeTypes[decl_idx] == CTType::Error()) continue;

            m_Solver.Reset();
            m_Targets.clear();

            const auto& sign = m_Ast.nodes[decl.data.func_decl.sign];
            std::string_view func_name = GetName(sign.data.func_sign.name);

            auto func = m_Table.Find(func_name);
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
                    AddError(ti.ident, { DiagType::RedeclaredIdentifier, param_name });
                }

                SetNodeType(param_idx, param_type);
                SetNodeType(ti.ident, param_type);
            }

            CheckBlock(decl.data.func_decl.body, CTType::Unit(), "", false);
            PopScope();
            ZN_CORE_ASSERT(m_CurrentDepth == 0, "Unbalanced scope depth after function check");
            ZN_CORE_ASSERT(m_Targets.empty(), "Unbalanced control target stack after function check");

            if (m_CurrentFunc.ret_type != CTType::Unit() &&
                m_CurrentFunc.ret_type != CTType::Error() &&
                !m_CurrentFunc.has_ret)
            {
                AddError(sign.data.func_sign.name, { DiagType::MissingReturn, func_name });
                SetNodeType(decl_idx, CTType::Error());
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
                    AddError(idx, { DiagType::CannotInferType });
                    m_NodeTypes[idx] = CTType::Error();
                }
                else
                {
                    m_NodeTypes[idx] = resolved;
                }
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
            {
                if (expected && !expected.IsVar())
                {
                    if (!Utils::IsInteger(expected)) ACTUAL(CTType::I64());
                    if (!Utils::CheckIntRange(n.data.i64, expected))
                        COMMIT_ERROR(DiagType::LiteralOutOfRange, {}, expected, CTType::I64());
                    ACTUAL(expected);
                }

                CTType v = m_Solver.Fresh();
                m_Solver.Emit({
                    .kind = Constraint::Kind::IntLit,
                    .lhs = v,
                    .lit = { .i64 = n.data.i64 },
                    .diag = { .node = node, .token = n.start_token, .diag = diag }
                });
                ACTUAL(v);
            }
            case NodeType::Decimal:
            {
                if (expected && !expected.IsVar())
                {
                    if (!Utils::IsFloat(expected)) ACTUAL(CTType::F64());
                    if (!Utils::CheckFloatRange(n.data.f64, expected))
                        COMMIT_ERROR(DiagType::LiteralOutOfRange, {}, expected, CTType::F64());
                    ACTUAL(expected);
                }

                CTType v = m_Solver.Fresh();
                m_Solver.Emit({
                    .kind = Constraint::Kind::FloatLit,
                    .lhs = v,
                    .lit = { .f64 = n.data.f64 },
                    .diag = { .node = node, .token = n.start_token, .diag = diag }
                });
                ACTUAL(v);
            }
            case NodeType::Bool: ACTUAL(CTType::Bool());
            case NodeType::ParenExpr: ACTUAL(ValidateExpr(n.data.child, expected, diag));
            case NodeType::Identifier:
            {
                auto name = GetName(node);
                auto symbol = FindSymbols(name);
                if (symbol.has_value()) ACTUAL(symbol->type);
                COMMIT_ERROR(DiagType::UnknownIdentifier, name);
            }
            case NodeType::Tuple:
            {
                // TODO: Update tuple support later
                COMMIT_ERROR(DiagType::UnsupportedFeature, "tuple expressions");
            }
            case NodeType::UnaryExpr:
            {
                uint32_t opnd_idx = n.data.unary.operand;
                // Fold unary '-' with integer literals so signed min bounds (e.g. -128 for i8) pass range checks.
                if (n.data.unary.op == Operator::Neg &&
                    opnd_idx != ERROR_NODE && opnd_idx < m_Ast.nodes.size() &&
                    m_Ast.nodes[opnd_idx].type == NodeType::Integer)
                {
                    int64_t neg_val = -m_Ast.nodes[opnd_idx].data.i64;
                    if (expected && !expected.IsVar())
                    {
                        if (!Utils::IsInteger(expected))
                        {
                            SetNodeType(opnd_idx, CTType::I64());
                            ACTUAL(CTType::I64());
                        }
                        if (!Utils::CheckIntRange(neg_val, expected))
                        {
                            SetNodeType(opnd_idx, CTType::Error());
                            COMMIT_ERROR(DiagType::LiteralOutOfRange, {}, expected, CTType::I64());
                        }
                        SetNodeType(opnd_idx, expected);
                        ACTUAL(expected);
                    }

                    CTType v = m_Solver.Fresh();
                    SetNodeType(opnd_idx, v);
                    m_Solver.Emit({
                        .kind = Constraint::Kind::IntLit,
                        .lhs = v,
                        .lit = { .i64 = neg_val },
                        .diag = { .node = node, .token = n.start_token, .diag = diag }
                    });
                    ACTUAL(v);
                }

                auto operand = ValidateExpr(opnd_idx, CTType::Unknown());
                if (operand == CTType::Error()) ERROR();

                if (!operand.IsVar())
                {
                    CTType result = Utils::CheckUnaryOp(n.data.unary.op, operand);
                    if (!result)
                    {
                        DiagType dt = (n.data.unary.op == Operator::Not || n.data.unary.op == Operator::Neg)
                            ? DiagType::OperandTypeMismatch
                            : DiagType::InvalidUnaryOp;
                        COMMIT_ERROR(dt, OperatorToString(n.data.unary.op), CTType::Error(), operand);
                    }
                    ACTUAL(result);
                }

                if (n.data.unary.op == Operator::Not)
                {
                    m_Solver.Emit({
                        .kind = Constraint::Kind::Eq,
                        .lhs = operand,
                        .rhs = CTType::Bool(),
                        .diag = { .args = std::string(OperatorToString(Operator::Not)), .node = node, .token = n.start_token, .diag = DiagType::OperandTypeMismatch }
                    });
                    ACTUAL(CTType::Bool());
                }
                if (n.data.unary.op == Operator::Neg)
                {
                    CTType res = (expected && !expected.IsVar()) ? expected : m_Solver.Fresh();
                    m_Solver.Emit({
                        .kind = Constraint::Kind::UnaryOp,
                        .lhs = operand,
                        .result = res,
                        .op = Operator::Neg,
                        .diag = { .node = node, .token = n.start_token, .diag = DiagType::TypeMismatch }
                    });
                    ACTUAL(res);
                }
                COMMIT_ERROR(DiagType::InvalidUnaryOp, OperatorToString(n.data.unary.op));
            }
            case NodeType::BinaryExpr:
            {
                auto lhs = ValidateExpr(n.data.binary.lhs, CTType::Unknown());
                CTType rhs_expected = (lhs.kind == CTType::Kind::Error ||
                                       lhs.kind == CTType::Kind::Unknown)
                    ? CTType::Unknown() : lhs;
                auto rhs = ValidateExpr(n.data.binary.rhs, rhs_expected);
                if (lhs == CTType::Error() || rhs == CTType::Error()) ERROR();

                if (!lhs.IsVar() && !rhs.IsVar())
                    ACTUAL(ResolveBinaryOp(node, n.data.binary.op, lhs, rhs));

                m_Solver.Emit({
                    .kind = Constraint::Kind::Eq,
                    .lhs = lhs,
                    .rhs = rhs,
                    .diag = { .args = std::string(OperatorToString(n.data.binary.op)), .node = node, .token = n.start_token, .diag = DiagType::InvalidBinaryOp }
                });

                Operator op = n.data.binary.op;
                bool is_bool_res = (op == Operator::Eq || op == Operator::Ne ||
                                    op == Operator::Lt || op == Operator::Lte ||
                                    op == Operator::Gt || op == Operator::Gte ||
                                    op == Operator::And || op == Operator::Or);
                CTType res = is_bool_res
                    ? CTType::Bool()
                    : ((expected && !expected.IsVar()) ? expected : m_Solver.Fresh());

                m_Solver.Emit({
                    .kind = Constraint::Kind::BinaryOp,
                    .lhs = lhs,
                    .rhs = rhs,
                    .result = res,
                    .op = op,
                    .diag = { .node = node, .token = n.start_token, .diag = DiagType::TypeMismatch }
                });
                ACTUAL(res);
            }
            case NodeType::CastExpr:
            {
                auto src = ValidateExpr(n.data.cast.expr, CTType::Unknown());
                auto dst = ResolveTypeNode(n.data.cast.type);
                if (src == CTType::Error() || dst == CTType::Error()) ERROR();

                if (!src.IsVar())
                {
                    if (Utils::ValidatePrimitiveCast(src, dst)) ACTUAL(dst);
                    COMMIT_ERROR(DiagType::CoerceFailure, {}, dst, src);
                }

                m_Solver.Emit({
                    .kind = Constraint::Kind::Cast,
                    .lhs = src,
                    .rhs = dst,
                    .diag = { .node = node, .token = n.start_token, .diag = DiagType::CoerceFailure }
                });
                ACTUAL(dst);
            }
            case NodeType::CallExpr:
            {
                if (n.data.call.callee == ERROR_NODE || n.data.call.callee >= m_Ast.nodes.size()) ERROR();
                // TODO: support general callable expr
                auto callee = m_Ast.nodes[n.data.call.callee].type;
                if (callee != NodeType::Identifier)
                    COMMIT_ERROR(DiagType::UnsupportedFeature, "indirect function calls");

                auto name = GetName(n.data.call.callee);
                auto sig = m_Table.Find(name);
                if (!sig.has_value())
                    COMMIT_ERROR(DiagType::UnknownIdentifier, name);

                if (n.data.call.args == ERROR_NODE || n.data.call.args >= m_Ast.nodes.size()) ERROR();
                uint32_t count = ListCount(n.data.call.args);
                if (count != sig->params.size())
                {
                    std::string args = std::to_string(sig->params.size()) + '\x1f' + std::to_string(count);
                    COMMIT_ERROR(DiagType::WrongArgCount, args);
                }

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
            case NodeType::IfExpr:    ACTUAL(CheckIf(node, expected));
            case NodeType::ForExpr:   ACTUAL(CheckFor(node, expected));
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
                    default: COMMIT_ERROR(DiagType::UnsupportedFeature, "labeled expression body");
                }
                break;
            }

            default: COMMIT_ERROR(DiagType::UnsupportedFeature, "expression");
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
                AddError(node, { diag, {}, expected, actual });
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
                AddError(node, { DiagType::BlockMissingBreak });
                return SetNodeType(node, CTType::Error());
            }
        }

        return SetNodeType(node, target_type);
    }

    CTType TypeChecker::CheckIf(uint32_t node, CTType expected, std::string_view label)
    {
        if (node == ERROR_NODE || node >= m_Ast.nodes.size()) return CTType::Error();
        if (expected == CTType::Error()) return CTType::Error();

        const auto& ie = m_Ast.nodes[node].data.if_expr;
        ValidateExpr(ie.cond, CTType::Bool(), DiagType::TypeMismatch);

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
        if (then_type == CTType::Error() || else_type == CTType::Error()) return SetNodeType(node, CTType::Error());

        return SetNodeType(node, res_type);
    }

    CTType TypeChecker::CheckFor(uint32_t node, CTType expected, std::string_view label)
    {
        if (node == ERROR_NODE || node >= m_Ast.nodes.size()) return CTType::Error();
        if (expected == CTType::Error()) return CTType::Error();

        const auto& fe = m_Ast.nodes[node].data.for_expr;
        bool has_cond = (fe.cond != ERROR_NODE);
        if (has_cond)
            ValidateExpr(fe.cond, CTType::Bool(), DiagType::TypeMismatch);

        CTType target_type = has_cond
            ? CTType::Unit()
            : ((expected != CTType::Unknown()) ? expected : m_Solver.Fresh());
        PushTarget({
            .label = label,
            .expected = target_type,
            .is_loop = true,
            .has_break = false
        });

        CheckBlock(fe.body, CTType::Unit(), "", false);
        bool has_break = PopTarget().has_break;

        if (!has_cond && !has_break)
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
                AddError(node, { DiagType::BlockMissingBreak });
                return SetNodeType(node, CTType::Error());
            }
        }

        return SetNodeType(node, target_type);
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
                    AddError(target, { DiagType::UnsupportedFeature, "let binding target" });
                    ValidateExpr(value, CTType::Unknown());
                    SetNodeType(node, CTType::Error());
                    break;
                }

                std::string_view name = GetName(ident);
                CTType var_type = (type != ERROR_NODE) ? ResolveTypeNode(type) : CTType::Unknown();
                CTType val_type = ValidateExpr(value, var_type, DiagType::LetTypeMismatch);
                if (type == ERROR_NODE) var_type = val_type;
                SetNodeType(node, var_type);
                SetNodeType(target, var_type);
                SetNodeType(ident, var_type);

                if (!AddSymbol({ name, m_CurrentDepth, var_type, is_mut }))
                {
                    AddError(ident, { DiagType::RedeclaredIdentifier, name });
                    SetNodeType(ident, CTType::Error());
                    SetNodeType(target, CTType::Error());
                    SetNodeType(node, CTType::Error());
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
                    AddError(target, { DiagType::InvalidAssignTarget });
                    ValidateExpr(value, CTType::Unknown());
                    SetNodeType(node, CTType::Error());
                    break;
                }

                std::string_view name = GetName(target);
                auto symbol = FindSymbols(name);
                if (!symbol.has_value())
                {
                    AddError(target, { DiagType::UnknownIdentifier, name });
                    ValidateExpr(value, CTType::Unknown());
                    SetNodeType(target, CTType::Error());
                    SetNodeType(node, CTType::Error());
                    break;
                }

                if (!symbol->is_mut)
                    AddError(target, { DiagType::ImmutableAssignment, name });

                CTType val_type = ValidateExpr(value, symbol->type, DiagType::AssignTypeMismatch);
                SetNodeType(target, symbol->type);
                SetNodeType(node, (!symbol->is_mut || val_type == CTType::Error()) ? CTType::Error() : CTType::Unit());
                break;
            }
            case NodeType::ReturnStmt:
            {
                m_CurrentFunc.has_ret = true;
                uint32_t expr = n.data.child;
                bool has_err = false;
                if (expr != ERROR_NODE)
                {
                    has_err = (ValidateExpr(expr, m_CurrentFunc.ret_type, DiagType::ReturnTypeMismatch) == CTType::Error());
                }
                else if (m_CurrentFunc.ret_type != CTType::Unit() && m_CurrentFunc.ret_type != CTType::Error())
                {
                    AddError(node, { DiagType::ReturnTypeMismatch, {}, m_CurrentFunc.ret_type, CTType::Unit() });
                    has_err = true;
                }
                SetNodeType(node, has_err ? CTType::Error() : CTType::Unit());
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
                        AddError(label, { DiagType::UndeclaredLabel, GetName(label) });
                    else
                        AddError(node, { DiagType::BreakOutsideLoop });

                    if (value != ERROR_NODE) ValidateExpr(value, CTType::Unknown());
                    SetNodeType(node, CTType::Error());
                    break;
                }

                target->has_break = true;
                bool has_err = false;
                if (value != ERROR_NODE)
                {
                    has_err = (ValidateExpr(value, target->expected, DiagType::BreakTypeMismatch) == CTType::Error());
                }
                else if (target->expected.IsVar())
                {
                    m_Solver.Emit({
                        .kind = Constraint::Kind::Eq,
                        .lhs = target->expected,
                        .rhs = CTType::Unit(),
                        .diag = { .node = node, .token = n.start_token, .diag = DiagType::BreakTypeMismatch }
                    });
                }
                else if (target->expected != CTType::Unit() && target->expected != CTType::Error())
                {
                    AddError(node, { DiagType::BreakTypeMismatch, {}, target->expected, CTType::Unit() });
                    has_err = true;
                }
                SetNodeType(node, has_err ? CTType::Error() : CTType::Unit());
                break;
            }
            case NodeType::ContinueStmt:
            {
                uint32_t label = n.data.continue_stmt.label;
                ControlTarget* target = FindContinue(label);
                if (!target)
                {
                    if (label != ERROR_NODE && !FindBreak(label))
                        AddError(label, { DiagType::UndeclaredLabel, GetName(label) });
                    else
                        AddError(node, { DiagType::InvalidContinue });
                    SetNodeType(node, CTType::Error());
                    break;
                }
                SetNodeType(node, CTType::Unit());
                break;
            }
            case NodeType::ExprStmt:
            {
                CTType expr_type = ValidateExpr(n.data.child, CTType::Unknown());
                SetNodeType(node, expr_type == CTType::Error() ? CTType::Error() : CTType::Unit());
                break;
            }
            case NodeType::FuncDecl:
            {
                AddError(node, { DiagType::UnsupportedFeature, "nested functions" });
                SetNodeType(node, CTType::Error());
                break;
            }
            default:
            {
                AddError(node, { DiagType::UnsupportedFeature, "statement" });
                SetNodeType(node, CTType::Error());
                break;
            }
        }
    }
}
