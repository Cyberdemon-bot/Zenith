#pragma once

#include "Zenith/Core/Base.h"
#include "Zenith/Compiler/Compiler.h"
#include "Zenith/Compiler/Ast.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Zenith {

    enum class Severity : uint8_t
    {
        Error,
        Warning,
        Note
    };

    enum class DiagType : uint8_t
    {
        // Pass 0 — collect signatures
        DuplicateFunction,
        UnknownTypeName,
        MissingParamType,            
        UnsupportedFeature, 

        // Pass 1 — infer
        MissingMainFunction,
        NestedFunction,
        UnknownIdentifier,
        OperandTypeMismatch,
        InvalidUnaryOp,
        WrongArgCount,
        CannotInferType,

        // Pass 2 — check
        LetTypeMismatch,
        ReturnTypeMismatch,
        ArgTypeMismatch,
        AssignTypeMismatch,
        CoerceFailure,

        // Meta
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

    struct FuncSign
    {
        // [params_start, params_count) / [rets_start, rets_count) → m_TPool
        uint32_t params_start, params_count;
        uint32_t rets_start,   rets_count;
        uint16_t protoId;
    };

    struct FuncView
    {
        std::span<const CTType> params;
        std::span<const CTType> rets;
        uint16_t                protoId;
    };

    class SignatureTable
    {
    public:
        std::optional<FuncView> Find(std::string_view name) const;

        void Add(std::string_view name,
                 std::span<const CTType> params = {},
                 std::span<const CTType> rets   = {},
                 uint16_t protoId = 0);

        // MUST be called before any Find().
        // Invalidates spans returned by previous Find()
        void Freeze();

    private:
        std::vector<CTType>   m_TPool;
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

        const std::vector<Diagnostic>& Diagnostics() const { return m_Errors; }

    private:
        AstView m_Ast;
        SignatureTable m_Table;
        uint32_t m_NextProtoId = 1; 
        uint16_t m_MainProtoId = INVALID_PROTO;
        bool m_Validated = false;

        std::vector<Diagnostic> m_Errors;

        void CollectSignature();
        uint32_t CollectParams(std::vector<CTType>& buffer, uint32_t params_list_node);
        uint32_t CollectRet(std::vector<CTType>& buffer, uint32_t ret_type_node);

        CTType ResolveTypeNode(uint32_t type_node);
        std::string_view IdentName(uint32_t ident_node) const;

        uint32_t ListCount(uint32_t list_node) const { return m_Ast.nodes[list_node].data.list.count; }
        uint32_t ListItem(uint32_t list_node, uint32_t i) const { return m_Ast.extra[m_Ast.nodes[list_node].data.list.start + i]; } 

        void AddError(uint32_t node, DiagType type, std::string_view message);
    };

}