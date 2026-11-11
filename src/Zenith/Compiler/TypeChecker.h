#pragma once

#include "Zenith/Core/Base.h"
#include "Zenith/Compiler/Compiler.h"
#include "Zenith/Compiler/Ast.h"

#include <span>
#include <vector>
#include <string>
#include <optional>
#include <string_view>
#include <unordered_map>

namespace Zenith {

    enum class Severity : uint8_t
    {
        Error,
        Warning,
        Note
    };

    enum class DiagType : uint8_t
    {
        // Declaration & Signature
        DuplicateFunction,
        MissingMainFunction,
        InvalidMainSignature,
        NestedFunction,
        MissingParamType,
        UnknownTypeName,

        // Scope & Symbol
        UnknownIdentifier,
        RedeclaredIdentifier,

        // Expression & Operation
        OperandTypeMismatch,
        InvalidUnaryOp,
        WrongArgCount,
        CannotInferType,
        CoerceFailure,

        // Type Mismatch
        LetTypeMismatch,
        ReturnTypeMismatch,
        ArgTypeMismatch,
        AssignTypeMismatch,
        BreakTypeMismatch,

        // Statement & Control Flow
        ImmutableAssignment,
        InvalidAssignTarget,
        UndeclaredLabel,
        InvalidContinue,
        BlockMissingBreak,

        // General
        UnsupportedFeature,
        TooManyErrors,
    };

    struct Diagnostic
    {
        std::string message;
        uint32_t node, token;            
        CTType expected = CTType::Error, actual = CTType::Error;
        Severity severity = Severity::Error;
        DiagType diag;
    };

    struct Error
    {
        DiagType type;
        std::string_view message;
    }; 

    struct FuncSign
    {
        uint32_t params_start, params_count;
        CTType ret_type = CTType::Unit;
        uint16_t protoId;
    };

    struct FuncView
    {
        std::span<const CTType> params;
        CTType ret_type = CTType::Unit;
        uint16_t protoId;
    };

    struct Symbol
    {
        std::string_view name;
        uint32_t depth;
        CTType type;
        bool is_mut;
    };

    struct FuncContext
    {
        CTType ret_type = CTType::Unit;
        bool has_ret = false;
    };

    struct ControlTarget
    {
        uint32_t node;           
        std::string_view label;  
        CTType expected;    
        bool is_loop, has_break = false; 
    };

    class SignatureTable
    {
    public:
        std::optional<FuncView> Find(std::string_view name) const;

        void Add(std::string_view name,
                 std::span<const CTType> params = {},
                 CTType ret_type = CTType::Unit,
                 uint16_t protoId = 0);

        // MUST be called before any Find().
        // Invalidates spans returned by previous Find()
        void Freeze();

    private:
        std::vector<CTType> m_TPool;
        std::vector<FuncSign> m_Signs;
        std::unordered_map<std::string_view, uint32_t> m_FuncIndex;

        bool m_Frozen = false;
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

        void PushScope();
        void PopScope();
        bool AddSymbol(const Symbol& sb);
        std::optional<Symbol> FindSymbols(std::string_view name);

        void PushTarget(const ControlTarget& target);
        void PopTarget();
        ControlTarget GetCurrentTarget();
        ControlTarget* FindBreak(uint32_t label);
        ControlTarget* FindContinue(uint32_t label);

        CTType InferExpr(uint32_t node);
        bool CheckExpr(uint32_t node, CTType expected, Error err);
        bool CheckStmt(uint32_t node);

        void CollectSignature();
        uint32_t CollectParams(std::vector<CTType>& buffer, uint32_t params_list_node);
        CTType CollectRet(uint32_t ret_type_node);

        CTType ResolveTypeNode(uint32_t type_node);
        std::string_view GetName(uint32_t node) const;

        constexpr uint32_t ListCount(uint32_t list_node) const { return m_Ast.nodes[list_node].data.list.count; }
        constexpr uint32_t ListItem(uint32_t list_node, uint32_t i) const { return m_Ast.extra[m_Ast.nodes[list_node].data.list.start + i]; } 

        void AddError(uint32_t node, Error err);
    };
}