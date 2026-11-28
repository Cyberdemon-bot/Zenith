#include "Zenith/Compiler/CTType.h"
#include "Zenith/Compiler/CodeGen.h"
#include "Zenith/Compiler/Node.h"
#include "Zenith/Compiler/SignatureTable.h"
#include "Zenith/Compiler/Utils.h"
#include "Zenith/Core/Assert.h"
#include "Zenith/Core/Base.h"
#include "Zenith/VM/Format.h"
#include "Zenith/VM/Instruction.h"
#include "Zenith/VM/Opcode.h"

#include <algorithm>
#include <optional>
#include <string_view>
#include <utility>

namespace Zenith {

    namespace {

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

    }

    void CodeGen::CompileFunc(const FuncView& func)
    {
        m_Builder.Clear();
        m_Context.Reset();
        m_Scopes.clear();
        m_ConstCache.clear();
        m_Func = { 
            .name = func.name, 
            .failed = false, 
            .ret_type = func.ret_type 
        };

        if (func.decl_node == ERROR_NODE) return;
        if (func.params.size() > INVALID_REG)
        {
            AddError(func.decl_node, DiagType::RegisterOverflow);
            return;
        }

        const auto& decl = GetNode(func.decl_node).data.func_decl;
        const auto& sign = GetNode(decl.sign).data.func_sign;

        PushScope();
        for (uint32_t i = 0; i < func.params.size(); ++i)
        {
            const uint32_t param_node = ListItem(sign.params, i);
            std::string_view param_name = GetName(GetNode(param_node).data.typed_ident.ident);
            m_Context.locals.push_back(LocalVar
            {
                .name = param_name,
                .depth = m_Context.depth,
                .reg = static_cast<uint8_t>(i),
                .type = func.params[i],
            });
        }
        m_Context.local_top = m_Context.reg_top = m_Context.max_regs = static_cast<uint16_t>(func.params.size());

        CompileBlock(decl.body, 0, "", false);
        if (m_Func.failed) return;

        // safe guard for unit ret_type
        m_Builder.CommitInstruction(
            Instruction::Make<Format::iABC>(
                Opcode::Return, uint8_t(0), uint8_t(0), uint8_t(0)));

        PopScope();
        ZN_CORE_ASSERT(m_Context.depth == 0 && m_Scopes.empty(), "Unbalanced scope after function");

        m_Builder.SetNumParams(static_cast<uint8_t>(func.params.size()));
        m_Builder.SetNumReturn(func.ret_type == CTType::Unit() ? 0 : 1);
        m_Builder.SetMaxRegs(static_cast<uint8_t>(m_Context.max_regs));
    }

    void CodeGen::CompileStmt(uint32_t node)
    {
        if (m_Func.failed) return;

        const auto& n = GetNode(node);
        switch (n.type)
        {
            case NodeType::LetStmt:
            {
                const auto& ls = n.data.let;
                const uint32_t ident_node = (GetNode(ls.target).type == NodeType::TypedIdentifier)
                                          ? GetNode(ls.target).data.typed_ident.ident
                                          : ls.target;
                CTType var_type = TypeOf(node);
                uint8_t var_reg = (var_type == CTType::Unit()) ? 0 : AllocLocal(node);

                if (m_Func.failed) return;
                if (ls.value != ERROR_NODE)
                {
                    CompileExpr(ls.value, var_reg);
                    if (m_Func.failed) return;
                }

                m_Context.locals.push_back(LocalVar
                {
                    .name = GetName(ident_node),
                    .depth = m_Context.depth,
                    .reg = var_reg,
                    .type = var_type,
                });
                break;
            }
            case NodeType::AssignStmt:
            {
                const auto& as = n.data.assign;
                const LocalVar* var = FindLocal(GetName(as.target));
                ZN_CORE_ASSERT(var != nullptr, "Undefined local variable");
                CompileExpr(as.value, (var->type == CTType::Unit()) ? 0 : var->reg);
                break;
            }
            case NodeType::ReturnStmt:
            {
                if (m_Func.ret_type == CTType::Unit())
                {
                    if (n.data.child != ERROR_NODE)
                    {
                        CompileExpr(n.data.child, 0);
                        if (m_Func.failed) return;
                    }
                    m_Builder.CommitInstruction(
                        Instruction::Make<Format::iABC>(Opcode::Return, uint8_t(0), uint8_t(0), uint8_t(0)));
                }
                else
                {
                    TempGuard guard(m_Context.reg_top);
                    uint8_t ret_reg = CompileOperand(n.data.child);
                    if (m_Func.failed) return;
                    m_Builder.CommitInstruction(
                        Instruction::Make<Format::iABC>(Opcode::Return, ret_reg, uint8_t(1), uint8_t(0)));
                }
                break;
            }
            case NodeType::BreakStmt:
            {
                const auto& br = n.data.break_stmt;
                const size_t idx = FindBreak(br.label);
                const uint8_t target_dst = m_Scopes[idx].dst;
                if (br.value != ERROR_NODE)
                {
                    CompileExpr(br.value, target_dst);
                    if (m_Func.failed) return;
                }
                uint32_t jmp_pc = EmitJump(Opcode::Jmp, 0);
                m_Scopes[idx].break_jmps.push_back(jmp_pc);
                break;
            }
            case NodeType::ContinueStmt:
            {
                const size_t idx = FindContinue(n.data.continue_stmt.label);
                uint32_t jmp_pc = EmitJump(Opcode::Jmp, 0);
                PatchJump(jmp_pc, m_Scopes[idx].loop_start_pc, node);
                break;
            }
            case NodeType::ExprStmt:
            {
                const uint32_t expr = n.data.child;
                if (TypeOf(expr) == CTType::Unit()) CompileExpr(expr, 0);
                else
                {
                    TempGuard guard(m_Context.reg_top);
                    uint8_t tmp = AllocTemp(expr);
                    if (m_Func.failed) return;
                    CompileExpr(expr, tmp);
                }
                break;
            }
            default: ZN_CORE_ASSERT(false, "Unexpected statement node"); break;
        }
    }

    void CodeGen::CompileBlock(uint32_t node, uint8_t dst, std::string_view label, bool push_target)
    {
        if (m_Func.failed) return;

        PushScope();
        if (push_target) PushControl(ControlScope
        {
            .label = label,
            .dst = dst,
            .is_loop = false
        });

        const uint32_t count = ListCount(node);
        for (uint32_t i = 0; i < count; ++i)
        {
            CompileStmt(ListItem(node, i));
            if (m_Func.failed) break;
        }

        if (push_target) PopControl(node);
        PopScope();
    }

    void CodeGen::CompileBranch(uint32_t node, uint8_t dst)
    {
        if (GetNode(node).type == NodeType::BlockExpr) CompileBlock(node, dst);
        else CompileExpr(node, dst);
    }

    void CodeGen::CompileIf(uint32_t node, uint8_t dst, std::string_view label)
    {
        if (m_Func.failed) return;

        const auto& ie = GetNode(node).data.if_expr;
        uint32_t jz_else = 0;
        {
            TempGuard guard(m_Context.reg_top);
            uint8_t cond_reg = CompileOperand(ie.cond);
            if (m_Func.failed) return;
            jz_else = EmitJump(Opcode::Jz, cond_reg);
        }

        if (!label.empty())
            PushControl(ControlScope
            { 
                .label = label, 
                .dst = dst, 
                .is_loop = false
            });

        CompileBranch(ie.then_branch, dst);
        if (m_Func.failed) return;

        if (ie.else_branch != ERROR_NODE)
        {
            uint32_t jmp_end = EmitJump(Opcode::Jmp, 0);
            PatchJump(jz_else, m_Builder.GetCodeSize(), node);
            if (m_Func.failed) return;

            CompileBranch(ie.else_branch, dst);
            if (m_Func.failed) return;

            PatchJump(jmp_end, m_Builder.GetCodeSize(), node);
        }
        else PatchJump(jz_else, m_Builder.GetCodeSize(), node);

        if (!label.empty()) PopControl(node);
    }

    void CodeGen::CompileFor(uint32_t node, uint8_t dst, std::string_view label)
    {
        if (m_Func.failed) return;

        const auto& fe = GetNode(node).data.for_expr;
        const uint32_t loop_start = m_Builder.GetCodeSize();

        PushControl(ControlScope
        {
            .label = label,
            .loop_start_pc = loop_start,
            .dst = dst,
            .is_loop = true,
        });

        if (fe.cond != ERROR_NODE)
        {
            TempGuard guard(m_Context.reg_top);
            uint8_t cond_reg = CompileOperand(fe.cond);
            if (m_Func.failed) return;
            uint32_t jz_exit = EmitJump(Opcode::Jz, cond_reg);
            m_Scopes.back().break_jmps.push_back(jz_exit);
        }

        CompileBlock(fe.body, dst, "", false);
        if (m_Func.failed) return;

        uint32_t back_pc = EmitJump(Opcode::Jmp, 0);
        PatchJump(back_pc, loop_start, node);
        if (m_Func.failed) return;

        PopControl(node);
    }

    void CodeGen::CompileExpr(uint32_t node, uint8_t dst)
    {
        if (m_Func.failed) return;

        const auto& n = GetNode(node);
        switch (n.type)
        {
            case NodeType::Bool:
            {
                m_Builder.CommitInstruction(
                    Instruction::Make<Format::iAsBx>(
                        Opcode::LoadImm, dst, static_cast<int16_t>(n.data.i64 ? 1 : 0)));
                break;
            }
            case NodeType::Identifier:
            {
                const auto* var = FindLocal(GetName(node));
                ZN_CORE_ASSERT(var != nullptr, "Undefined local variable");
                if (var->type != CTType::Unit() && dst != var->reg)
                    m_Builder.CommitInstruction(
                        Instruction::Make<Format::iABC>(
                            Opcode::Mov, dst, var->reg, uint8_t(0)));
                break;
            }
            case NodeType::Tuple:
            {
                ZN_CORE_ASSERT(ListCount(node) == 0, "Non-unit tuple is unsupported");
                break;
            }
            case NodeType::Decimal:    EmitFloatConst(dst, n.data.f64, TypeOf(node), node); break;
            case NodeType::Integer:    EmitIntConst(dst, n.data.i64, TypeOf(node), node); break;
            case NodeType::ParenExpr:  CompileExpr(n.data.child, dst); break;
            case NodeType::BinaryExpr: CompileBinary(node, dst); break;
            case NodeType::UnaryExpr:  CompileUnary(node, dst); break;
            case NodeType::CastExpr:   CompileCast(node, dst); break;
            case NodeType::CallExpr:   CompileCall(node, dst); break;
            
            case NodeType::BlockExpr:  CompileBlock(node, dst, "", true); break;
            case NodeType::IfExpr:     CompileIf(node, dst); break;
            case NodeType::ForExpr:    CompileFor(node, dst); break;
            case NodeType::LabeledExpr:
            {
                std::string_view label = GetName(n.data.labeled.label);
                uint32_t body = n.data.labeled.body;
                switch (GetNode(body).type)
                {
                    case NodeType::BlockExpr: CompileBlock(body, dst, label, true); break;
                    case NodeType::IfExpr:    CompileIf(body, dst, label); break;
                    case NodeType::ForExpr:   CompileFor(body, dst, label); break;
                    default: ZN_CORE_ASSERT(false, "Invalid labeled target"); break;
                }
                break;
            }
            default: ZN_CORE_ASSERT(false, "Unexpected expression node"); return;
        }
    }

    void CodeGen::CompileCast(uint32_t node, uint8_t dst)
    {
        const auto& cast = GetNode(node).data.cast;
        CTType src_ct = TypeOf(cast.expr);
        CTType dst_ct = TypeOf(node);

        CompileExpr(cast.expr, dst);
        if (m_Func.failed || src_ct == dst_ct) return;

        if (src_ct == CTType::Bool()) return; 
        if (dst_ct == CTType::Bool())
        {
            m_Builder.CommitInstruction(
                Instruction::Make<Format::iABC>(Opcode::Cast, dst, dst, static_cast<uint8_t>(CastKind::ToBool)));
            return;
        }
        if (auto kind = FindNumericCast(ToVMType(src_ct), ToVMType(dst_ct)))
        {
            m_Builder.CommitInstruction(
                Instruction::Make<Format::iABC>(Opcode::Cast, dst, dst, static_cast<uint8_t>(*kind)));
        }
        EmitTruncIfNeeded(dst, dst_ct);
    }

    void CodeGen::CompileCall(uint32_t node, uint8_t dst)
    {
        const auto& call = GetNode(node).data.call;
        std::string_view callee_name = GetName(call.callee);
        auto callee = m_Typed.signatures->Find(callee_name);
        ZN_CORE_ASSERT(callee.has_value(), "Unknown function");

        const uint32_t arg_count = ListCount(call.args);
        const bool has_ret = (callee->ret_type != CTType::Unit());
        const uint16_t window_size = std::max<uint16_t>(static_cast<uint16_t>(arg_count), has_ret ? 1 : 0);

        const uint16_t base = m_Context.reg_top;
        if (base + window_size > INVALID_REG)
        {
            AddError(node, DiagType::RegisterOverflow);
            return;
        }
        m_Context.max_regs = std::max<uint16_t>(m_Context.max_regs, base + window_size);

        TempGuard guard(m_Context.reg_top);
        for (uint32_t i = 0; i < arg_count; ++i)
        {
            uint8_t arg_reg = static_cast<uint8_t>(base + i);
            m_Context.reg_top = static_cast<uint16_t>(base + i + 1);
            CompileExpr(ListItem(call.args, i), arg_reg);
            if (m_Func.failed) return;
        }

        m_Builder.CommitInstruction(
            Instruction::Make<Format::iABx>(
                Opcode::Call, static_cast<uint8_t>(base), callee->protoId));

        if (has_ret && dst != base)
            m_Builder.CommitInstruction(
                Instruction::Make<Format::iABC>(
                    Opcode::Mov, dst, static_cast<uint8_t>(base), uint8_t(0)));
    }

    void CodeGen::CompileUnary(uint32_t node, uint8_t dst)
    {
        const auto& un = GetNode(node).data.unary;
        if (un.op == Operator::Neg)
        {
            uint32_t opnd = un.operand;
            while (GetNode(opnd).type == NodeType::ParenExpr)
                opnd = GetNode(opnd).data.child;

            if (GetNode(opnd).type == NodeType::Integer)
            {
                int64_t neg = static_cast<int64_t>(0ull - static_cast<uint64_t>(GetNode(opnd).data.i64));
                EmitIntConst(dst, neg, TypeOf(node), node);
                return;
            }
        }

        CompileExpr(un.operand, dst);
        if (m_Func.failed) return;

        switch (un.op)
        {
            case Operator::Neg:
            {
                CTType ty = TypeOf(node);
                m_Builder.CommitInstruction(
                    Instruction::Make<Format::iABC>(
                        Opcode::Neg, ToVMType(ty), dst, dst, uint8_t(0)));
                EmitTruncIfNeeded(dst, ty);
                break;
            }
            case Operator::Not:
            {
                m_Builder.CommitInstruction(
                    Instruction::Make<Format::iABC>(
                        Opcode::LNot, dst, dst, uint8_t(0)));
                break;
            }
            default: break;
        }
    }

    void CodeGen::CompileBinary(uint32_t node, uint8_t dst)
    {
        const auto& bin = GetNode(node).data.binary;

        // short-circuit
        if (bin.op == Operator::And || bin.op == Operator::Or)
        {
            CompileExpr(bin.lhs, dst);
            if (m_Func.failed) return;

            Opcode jmp_op = (bin.op == Operator::And) ? Opcode::Jz : Opcode::Jnz;
            uint32_t skip_pc = EmitJump(jmp_op, dst);

            CompileExpr(bin.rhs, dst);
            if (m_Func.failed) return;

            PatchJump(skip_pc, m_Builder.GetCodeSize(), node);
            return;
        }

        // arithmetic and comparison
        TempGuard guard(m_Context.reg_top);
        uint8_t lhs = CompileOperand(bin.lhs);
        uint8_t rhs = CompileOperand(bin.rhs);
        if (m_Func.failed) return;

        CTType op_ct = TypeOf(bin.lhs);
        Type vm_ty = ToVMType(op_ct);
        Opcode op = Opcode::Add;
        bool is_arith = true;

        switch (bin.op)
        {
            case Operator::Add: op = Opcode::Add; break;
            case Operator::Sub: op = Opcode::Sub; break;
            case Operator::Mul: op = Opcode::Mul; break;
            case Operator::Div:
                op = Utils::IsFloat(op_ct)
                   ? Opcode::Div
                   : (Utils::IsSignedInteger(op_ct) ? Opcode::DivS : Opcode::DivU);
                break;
            case Operator::Mod:
                op = Utils::IsSignedInteger(op_ct) ? Opcode::RemS : Opcode::RemU;
                break;

            case Operator::Eq:  op = Opcode::Eq; is_arith = false; break;
            case Operator::Ne:  op = Opcode::Ne; is_arith = false; break;
            case Operator::Lt:  op = Opcode::Lt; is_arith = false; break;
            case Operator::Lte: op = Opcode::Le; is_arith = false; break;
            case Operator::Gt:  op = Opcode::Lt; std::swap(lhs, rhs); is_arith = false; break;
            case Operator::Gte: op = Opcode::Le; std::swap(lhs, rhs); is_arith = false; break;

            default: ZN_CORE_ASSERT(false, "Unexpected binary operator"); return;
        }

        m_Builder.CommitInstruction(
            Instruction::Make<Format::iABC>(op, vm_ty, dst, lhs, rhs));

        if (is_arith)
            EmitTruncIfNeeded(dst, op_ct);
    }

    uint8_t CodeGen::CompileOperand(uint32_t expr)
    {
        if (m_Func.failed) return INVALID_REG;

        while (GetNode(expr).type == NodeType::ParenExpr) // unparen
            expr = GetNode(expr).data.child;

        if (GetNode(expr).type == NodeType::Identifier)
        {
            const auto* var = FindLocal(GetName(expr));
            ZN_CORE_ASSERT(var != nullptr, "Undefined local variable");
            return var->reg;
        }

        uint8_t temp = AllocTemp(expr);
        if (temp == INVALID_REG) return INVALID_REG;

        CompileExpr(expr, temp);
        return temp;
    }

}
