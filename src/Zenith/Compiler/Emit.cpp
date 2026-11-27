#include "Zenith/Compiler/CodeGen.h"
#include "Zenith/Compiler/Node.h"
#include "Zenith/Compiler/Utils.h"
#include "Zenith/Core/Assert.h"
#include "Zenith/Core/Base.h"
#include "Zenith/VM/Format.h"
#include "Zenith/VM/Instruction.h"
#include "Zenith/VM/Opcode.h"

#include <utility>

namespace Zenith {

    void CodeGen::CompileFunc(const FuncView& func)
    {

    }

    void CodeGen::CompileStmt(uint32_t node)
    {

    }

    void CodeGen::CompileBlock(uint32_t node, uint8_t dst, std::string_view label, bool push_target)
    {

    }

    void CodeGen::CompileIf(uint32_t node, uint8_t dst, std::string_view label)
    {

    }

    void CodeGen::CompileFor(uint32_t node, uint8_t dst, std::string_view label)
    {
        
    }

    void CodeGen::CompileExpr(uint32_t node, uint8_t dst)
    {
        if (m_Func.failed) return;

        const auto& n = GetNode(node);
        switch (n.type)
        {
            case NodeType::Integer:
            {
                EmitIntConst(dst, n.data.i64, TypeOf(node), node);
                break;
            }
            case NodeType::Decimal:
            {
                EmitFloatConst(dst, n.data.f64, TypeOf(node), node);
                break;
            }
            case NodeType::Bool:
            {
                m_Builder.CommitInstruction(
                    Instruction::Make<Format::iAsBx>(
                        Opcode::LoadImm, dst, static_cast<int16_t>(n.data.i64 ? 1 : 0)));
                break;
            }
            case NodeType::Tuple:
            {
                ZN_CORE_ASSERT(ListCount(node) == 0, "tuple is unsupported");
                break;
            }
            case NodeType::ParenExpr:
            {
                CompileExpr(n.data.child, dst);
                break;
            }
            case NodeType::Identifier:
            {
                const auto* var = FindLocal(GetName(node));
                ZN_CORE_ASSERT(var != nullptr, "CompileExpr: undefined local variable");
                if (var->type != CTType::Unit() && dst != var->reg)
                    m_Builder.CommitInstruction(
                        Instruction::Make<Format::iABC>(
                            Opcode::Mov, dst, var->reg, uint8_t(0)));
                break;
            }
            case NodeType::BinaryExpr:
            {
                CompileBinary(node, dst);
                break;
            }
            case NodeType::UnaryExpr:
            {
                CompileUnary(node, dst);
                break;
            }
            default: break;
        }
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
                        Opcode::Neg, Utils::ToVMType(ty), dst, dst, uint8_t(0)));
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
        Type vm_ty = Utils::ToVMType(op_ct);
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

            default: ZN_CORE_ASSERT(false, "CompileBinary: unexpected operator"); return;
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
            ZN_CORE_ASSERT(var != nullptr, "CompileOperand: undefined local variable");
            return var->reg;
        }

        uint8_t temp = AllocTemp(expr);
        if (temp == INVALID_REG) return INVALID_REG;

        CompileExpr(expr, temp);
        return temp;
    }

}
