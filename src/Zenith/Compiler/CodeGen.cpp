#include "Zenith/Compiler/CodeGen.h"
#include "Zenith/Compiler/SignatureTable.h"
#include "Zenith/Compiler/Utils.h"
#include "Zenith/Core/Assert.h"
#include "Zenith/Core/Base.h"
#include "Zenith/VM/Format.h"
#include "Zenith/VM/Instruction.h"
#include "Zenith/VM/Opcode.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <ranges>

namespace Zenith {

    void CodeGen::Clear()
    {
        m_Typed = {};
        m_Builder.Clear();
        m_Errors.Clear();
        m_Context.Reset();
        m_Func = {};
        m_ConstCache.clear();
    }

    bool CodeGen::Generate(TypedAstView ast, Module& out)
    {
        ZN_CORE_ASSERT(ast.signatures != nullptr, "TypedAstView signatures cannot be null");
        Clear();
        out.Clear();
        m_Typed = ast;
        out.Reserve(static_cast<uint16_t>(m_Typed.signatures->Count()));

        bool is_valid = true;
        m_Typed.signatures->ForEach([this, &out, &is_valid](const FuncView& view)
        {
            CompileFunc(view);
            if (m_Func.failed) { is_valid = false; return; }
            out.Fill(view.protoId, m_Builder);
        });

        if (!is_valid || HasErrors()) return false;
        out.SetEntry(m_Typed.main_proto_id);
        out.Freeze();
        return true;
    }

    const Node& CodeGen::GetNode(uint32_t idx) const
    {
        ZN_CORE_ASSERT(idx < m_Typed.ast.nodes.size(), "GetNode: index out of bounds");
        return m_Typed.ast.nodes[idx];
    }

    CTType CodeGen::TypeOf(uint32_t idx) const
    {
        ZN_CORE_ASSERT(idx < m_Typed.node_types.size(), "TypeOf: index out of bounds");
        return m_Typed.node_types[idx];
    }

    std::string_view CodeGen::GetName(uint32_t node) const
    {
        std::string_view name;
        [[maybe_unused]] bool ok = Utils::GetName(m_Typed.ast, node, name);
        ZN_CORE_ASSERT(ok, "GetName: expected valid Identifier or Label node");
        return name;
    }

    uint32_t CodeGen::ListCount(uint32_t list_node) const
    {
        uint32_t count = 0;
        [[maybe_unused]] bool ok = Utils::ListCount(m_Typed.ast, list_node, count);
        ZN_CORE_ASSERT(ok, "ListCount: invalid list node");
        return count;
    }

    uint32_t CodeGen::ListItem(uint32_t list_node, uint32_t i) const
    {
        uint32_t item = 0;
        [[maybe_unused]] bool ok = Utils::ListItem(m_Typed.ast, list_node, i, item);
        ZN_CORE_ASSERT(ok, "ListItem: invalid list node or index out of bounds");
        return item;
    }

    void CodeGen::PushScope()
    {
        ++m_Context.depth;
    }

    void CodeGen::PopScope()
    {
        ZN_CORE_ASSERT(m_Context.depth > 0, "Scope underflow");
        while (!m_Context.locals.empty() &&
               m_Context.locals.back().depth == m_Context.depth)
        {
            const auto& top = m_Context.locals.back();
            if (top.type != CTType::Unit()) m_Context.local_top = top.reg;
            m_Context.locals.pop_back();
        }
        m_Context.reg_top = m_Context.local_top;
        --m_Context.depth;
    }

    uint8_t CodeGen::AllocLocal(uint32_t node)
    {
        if (m_Context.local_top >= INVALID_REG) return AddError(node, DiagType::RegisterOverflow);
        uint8_t reg = static_cast<uint8_t>(m_Context.local_top++);
        m_Context.reg_top = std::max(m_Context.reg_top, m_Context.local_top);
        m_Context.max_regs = std::max(m_Context.max_regs, m_Context.reg_top);
        return reg;
    }

    uint8_t CodeGen::AllocTemp(uint32_t node)
    {
        if (m_Context.reg_top >= INVALID_REG) return AddError(node, DiagType::RegisterOverflow);
        uint8_t reg = static_cast<uint8_t>(m_Context.reg_top++);
        m_Context.max_regs = std::max(m_Context.max_regs, m_Context.reg_top);
        return reg;
    }

    const LocalVar* CodeGen::FindLocal(std::string_view name) const
    {
        for (const auto& var : m_Context.locals | std::views::reverse)
            if (var.name == name) return &var;
        return nullptr;
    }

    void CodeGen::EmitTruncIfNeeded(uint8_t reg, CTType type)
    {
        CastKind kind;
        switch (type.kind)
        {
            case CTType::Kind::I8:  kind = CastKind::TruncI8;  break;
            case CTType::Kind::I16: kind = CastKind::TruncI16; break;
            case CTType::Kind::U8:  kind = CastKind::TruncU8;  break;
            case CTType::Kind::U16: kind = CastKind::TruncU16; break;
            default: return;
        }
        m_Builder.CommitInstruction(
            Instruction::Make<Format::iABC>(
                Opcode::Cast, reg, reg, static_cast<uint8_t>(kind)));
    }

    void CodeGen::EmitLoadConst(uint8_t dst, Value val, uint32_t node)
    {
        uint16_t idx = 0;
        if (auto it = m_ConstCache.find(val.u64); it != m_ConstCache.end()) idx = it->second;
        else
        {
            if (m_Builder.GetConstSize() >= MAX_Bx)
            {
                AddError(node, DiagType::ConstantPoolOverflow);
                return;
            }
            idx = m_Builder.PushConstant(val);
            m_ConstCache.emplace(val.u64, idx);
        }
        m_Builder.CommitInstruction(
            Instruction::Make<Format::iABx>(
                Opcode::LoadConst, dst, idx));
    }

    void CodeGen::EmitIntConst(uint8_t dst, int64_t val, CTType type, uint32_t node)
    {
        const bool fits_i16 = (val >= -32768 && val <= 32767);
        if ((type == CTType::I64() && fits_i16) || (val >= 0 && val <= 32767))
        {
            m_Builder.CommitInstruction(
                Instruction::Make<Format::iAsBx>(
                    Opcode::LoadImm, dst, static_cast<int16_t>(val)));
            return;
        }

        Value v{};
        switch (type.kind)
        {
            case CTType::Kind::I8:
            case CTType::Kind::I16:
            case CTType::Kind::I32: v.SetAs<Type::I32>(static_cast<int32_t>(val)); break;
            case CTType::Kind::U8:
            case CTType::Kind::U16:
            case CTType::Kind::U32: v.SetAs<Type::U32>(static_cast<uint32_t>(val)); break;
            case CTType::Kind::I64: v.SetAs<Type::I64>(val); break;
            case CTType::Kind::U64: v.SetAs<Type::U64>(static_cast<uint64_t>(val)); break;
            default: ZN_CORE_ASSERT(false, "EmitIntConst: unexpected type"); break;
        }
        EmitLoadConst(dst, v, node);
    }

    void CodeGen::EmitFloatConst(uint8_t dst, double val, CTType type, uint32_t node)
    {
        Value v{};
        if (type == CTType::F32()) v.SetAs<Type::F32>(static_cast<float>(val));
        else v.SetAs<Type::F64>(val);
        EmitLoadConst(dst, v, node);
    }

    uint8_t CodeGen::AddError(uint32_t node, DiagType diag)
    {
        ZN_CORE_ASSERT(node < m_Typed.ast.nodes.size(), "AddError: invalid node index");
        const uint32_t token = m_Typed.ast.nodes[node].start_token;
        ZN_CORE_ASSERT(token < m_Typed.ast.tokens.size(), "AddError: invalid token index");

        m_Errors.Push(Diagnostic
        {
            .args = std::string(m_Func.name),
            .node = node,
            .token = token,
            .severity = GetDiagDef(diag).severity,
            .diag = diag,
        });
        m_Func.failed = true;

        return INVALID_REG;
    }

    uint32_t CodeGen::EmitJump(Opcode op, uint8_t reg)
    {
        ZN_CORE_ASSERT(op == Opcode::Jmp || op == Opcode::Jz || op == Opcode::Jnz,
                       "EmitJump: expected jump opcode");
        return m_Builder.CommitInstruction(
            Instruction::Make<Format::iAsBx>(
                op, reg, int16_t(0)));
    }

    void CodeGen::PatchJump(uint32_t jmp_pc, uint32_t target_pc, uint32_t node)
    {
        int64_t diff = static_cast<int64_t>(target_pc) - static_cast<int64_t>(jmp_pc + 1);
        if (diff < std::numeric_limits<int16_t>::min() ||
            diff > std::numeric_limits<int16_t>::max())
        {
            AddError(node, DiagType::JumpOffsetOverflow);
            return;
        }

        m_Builder.PatchJmp(jmp_pc, static_cast<int16_t>(diff));
    }

}
