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

    struct TypedAstView
    {
        AstView ast;
        std::span<const CTType> node_types;
        const SignatureTable* signatures = nullptr;
        uint16_t main_proto_id = INVALID_PROTO;
    };

    class TypeChecker
    {
    public:
        TypeChecker() = default;

        void Clear();
        TypedAstView Validate(AstView ast);

        bool HasErrors() const { return m_Errors.HasErrors(); }
        std::span<const Diagnostic> Diagnostics() const { return m_Errors.View(); }

    private:
        AstView m_Ast{};
        SignatureTable m_Table;
        ConstraintSolver m_Solver;
        uint32_t m_CurrentDepth = 0;
        uint32_t m_NextProtoId = 0;
        uint16_t m_MainProtoId = INVALID_PROTO;
        FuncContext m_CurrentFunc;

        ErrorStack m_Errors;
        std::vector<Symbol> m_Symbols;
        std::vector<ControlTarget> m_Targets;
        std::vector<CTType> m_Scratch;
        std::vector<CTType> m_NodeTypes;

        // checker
        CTType CheckBlock(uint32_t node, CTType expected, std::string_view label = "", bool push_target = false);
        CTType CheckIf(uint32_t node, CTType expected, std::string_view label = "");
        CTType CheckFor(uint32_t node, CTType expected, std::string_view label = "");
        void CheckStmt(uint32_t node);

        // scope management
        void PushScope();
        void PopScope();
        bool AddSymbol(const Symbol& sb);
        std::optional<Symbol> FindSymbols(std::string_view name);

        // control flow management
        void PushTarget(const ControlTarget& target);
        ControlTarget PopTarget();
        ControlTarget* FindBreak(uint32_t label);
        ControlTarget* FindContinue(uint32_t label);

        // collect phase
        void CollectSignature();
        uint32_t CollectParams(std::vector<CTType>& buffer, uint32_t params_list_node);
        CTType CollectRet(uint32_t ret_type_node);

        // helpers
        CTType SetNodeType(uint32_t node, CTType type);
        CTType ResolveTypeNode(uint32_t type_node);
        CTType ValidateIntLiteral(uint32_t node, int64_t val, CTType expected, DiagType diag);
        CTType ValidateExpr(uint32_t node, CTType expected, DiagType diag = DiagType::TypeMismatch);

        std::string_view GetName(uint32_t node) const;
        uint32_t ListCount(uint32_t list_node) const;
        uint32_t ListItem(uint32_t list_node, uint32_t i) const;

        void AddError(uint32_t node, Error err);
    };
}