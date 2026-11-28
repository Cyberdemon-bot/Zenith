#pragma once

#include "Zenith/Compiler/CTType.h"
#include "Zenith/Compiler/TypeChecker.h"
#include "Zenith/VM/Proto.h"

#include <cstdint>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Zenith {

    struct FuncView;

    struct LocalVar
    {
        std::string_view name;
        uint32_t depth;
        uint8_t reg;
        CTType type;
    };

    struct RegContext
    {
        std::vector<LocalVar> locals;
        uint32_t depth = 0;
        uint16_t local_top = 0, reg_top = 0, max_regs = 0; // uint16_t for overflow checking

        void Reset()
        {
            locals.clear();
            depth = 0;
            local_top = reg_top = max_regs = 0;
        }
    };

    struct TempGuard
    {
        uint16_t& top;
        uint16_t saved;

        explicit TempGuard(uint16_t& t) noexcept : top(t), saved(t) {}
        ~TempGuard() { top = saved; }

        TempGuard(const TempGuard&) = delete;
        TempGuard& operator=(const TempGuard&) = delete;
    };

    struct GenFuncContext
    {
        std::string_view name;
        bool failed = false;
        CTType ret_type = CTType::Unit();
    };

    struct ControlScope
    {
        std::string_view label;
        uint32_t loop_start_pc = 0;
        std::vector<uint32_t> break_jmps;
        uint8_t dst = 0; bool is_loop = false;
    };
    
    class CodeGen
    {
    public:
        void Clear();
        bool Generate(TypedAstView ast, Module& out);

        bool HasErrors() const { return m_Errors.HasErrors(); }
        std::span<const Diagnostic> Diagnostics() const { return m_Errors.View(); }
    private:
        TypedAstView m_Typed{};
        ProtoBuilder m_Builder;
        ErrorStack m_Errors;
        RegContext m_Context;
        GenFuncContext m_Func;
        std::vector<ControlScope> m_Scopes;
        std::unordered_map<uint64_t, uint16_t> m_ConstCache;

        // main process
        void CompileFunc(const FuncView& func);
        void CompileStmt(uint32_t node);
        void CompileExpr(uint32_t node, uint8_t dst);
        void CompileBlock(uint32_t node, uint8_t dst, std::string_view label = "", bool push_target = false);
        void CompileIf(uint32_t node, uint8_t dst, std::string_view label = "");
        void CompileFor(uint32_t node, uint8_t dst, std::string_view label = "");

        // sub process
        void CompileUnary(uint32_t node, uint8_t dst);
        void CompileBinary(uint32_t node, uint8_t dst);
        void CompileCast(uint32_t node, uint8_t dst);
        void CompileCall(uint32_t node, uint8_t dst);
        void CompileBranch(uint32_t node, uint8_t dst);
        uint8_t CompileOperand(uint32_t expr); 

        // emitters
        uint32_t EmitJump(Opcode op, uint8_t reg);
        void PatchJump(uint32_t jmp_pc, uint32_t target_pc, uint32_t node);
        void EmitTruncIfNeeded(uint8_t reg, CTType type);
        void EmitLoadConst(uint8_t dst, Value val, uint32_t node);
        void EmitIntConst(uint8_t dst, int64_t val, CTType type, uint32_t node);
        void EmitFloatConst(uint8_t dst, double val, CTType type, uint32_t node);

        // scope management
        void PushScope();
        void PopScope();

        void PushControl(const ControlScope& scope);
        void PopControl(uint32_t node);
        size_t FindBreak(uint32_t label) const;
        size_t FindContinue(uint32_t label) const;

        // helpers
        uint8_t AllocTemp(uint32_t node);
        uint8_t AllocLocal(uint32_t node);
        const LocalVar* FindLocal(std::string_view name) const;

        CTType TypeOf(uint32_t idx) const;
        const Node& GetNode(uint32_t idx) const;
        std::string_view GetName(uint32_t node) const;
        uint32_t ListCount(uint32_t list_node) const;
        uint32_t ListItem(uint32_t list_node, uint32_t i) const;

        uint8_t AddError(uint32_t node, DiagType diag);
    };
}
