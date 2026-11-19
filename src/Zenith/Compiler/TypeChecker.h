#pragma once

#include "Zenith/Core/Base.h"
#include "Zenith/Compiler/Ast.h"
#include "Zenith/Compiler/CTType.h"
#include "Zenith/Compiler/Diagnostic.h"
#include "Zenith/Compiler/SignatureTable.h"
#include "Zenith/Compiler/Constraint.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>


namespace Zenith {

    struct Symbol
    {
        std::string_view name;
        uint32_t depth;
        CTType type;
        bool is_mut;
    };

    struct FuncContext
    {
        CTType ret_type = CTType::Unit();
        bool has_ret = false;
    };

    struct ControlTarget
    {
        std::string_view label;
        CTType expected;
        bool is_loop = false;
        bool has_break = false;
    };

    class TypeChecker
    {
    public:
        explicit TypeChecker(AstView ast)
            : m_Ast(ast)
        {}

        // Returns true if no Severity::Error was emitted.
        bool Validate();

        const std::span<const Diagnostic> Diagnostics() const { return m_Errors; }

    private:
        AstView m_Ast;
        SignatureTable m_Table;
        ConstraintSolver m_Solver;
        uint32_t m_CurrentDepth = 0;
        uint32_t m_NextProtoId = 1; 
        uint16_t m_MainProtoId = INVALID_PROTO;
        FuncContext m_CurrentFunc;
        bool m_Validated = false;

        std::vector<Diagnostic> m_Errors;
        std::vector<Symbol> m_Symbols;
        std::vector<ControlTarget> m_Targets;
        std::vector<CTType> m_Scratch;
        std::vector<CTType> m_NodeTypes;

        CTType CheckBlock(uint32_t node, CTType expected, std::string_view label = "", bool push_target = false);
        void CheckStmt(uint32_t node);

        void PushScope();
        void PopScope();
        bool AddSymbol(const Symbol& sb);
        std::optional<Symbol> FindSymbols(std::string_view name);

        void PushTarget(const ControlTarget& target);
        ControlTarget PopTarget();
        ControlTarget* FindBreak(uint32_t label);
        ControlTarget* FindContinue(uint32_t label);

        CTType SetNodeType(uint32_t node, CTType type);
        CTType ResolveTypeNode(uint32_t type_node);
        CTType ResolveBinaryOp(uint32_t node, Operator op, CTType lhs, CTType rhs);
        CTType ValidateExpr(uint32_t node, CTType expected, DiagType diag = DiagType::OperandTypeMismatch);

        void CollectSignature();
        uint32_t CollectParams(std::vector<CTType>& buffer, uint32_t params_list_node);
        CTType CollectRet(uint32_t ret_type_node);

        std::string_view GetName(uint32_t node) const;

        constexpr uint32_t ListCount(uint32_t list_node) const { return m_Ast.nodes[list_node].data.list.count; }
        constexpr uint32_t ListItem(uint32_t list_node, uint32_t i) const { return m_Ast.extra[m_Ast.nodes[list_node].data.list.start + i]; } 

        void AddError(uint32_t node, Error err);
    };
}