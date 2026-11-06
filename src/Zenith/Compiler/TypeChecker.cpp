#include "Zenith/Core/Assert.h"
#include "Zenith/Compiler/Node.h"
#include "Zenith/Compiler/TypeChecker.h"

namespace Zenith {

    std::optional<FuncView> SignatureTable::Find(std::string_view name) const
    {
        auto it = m_FuncIndex.find(name);
        if (it == m_FuncIndex.end()) return std::nullopt;

        const FuncSign& sig = m_Signs[it->second];
        return FuncView
        {
            .params = { m_TPool.data() + sig.params_start, sig.params_count },
            .rets = { m_TPool.data() + sig.rets_start, sig.rets_count },
            .protoId = sig.protoId,
        };
    }

    void SignatureTable::Add(std::string_view name,
                            std::span<const CTType> params,
                            std::span<const CTType> rets,
                            uint16_t protoId)
    {
        ZN_CORE_ASSERT(!m_Frozen, "Cannot add signature: table is frozen");
        ZN_CORE_ASSERT(m_FuncIndex.find(name) == m_FuncIndex.end(),
                    "duplicate function signature added");

        const uint32_t pOff = static_cast<uint32_t>(m_TPool.size());
        m_TPool.insert(m_TPool.end(), params.begin(), params.end());

        const uint32_t rOff = static_cast<uint32_t>(m_TPool.size());
        m_TPool.insert(m_TPool.end(), rets.begin(), rets.end());

        const uint32_t sigIdx = static_cast<uint32_t>(m_Signs.size());
        m_Signs.push_back(FuncSign
        {
            .params_start = pOff,
            .params_count = static_cast<uint32_t>(params.size()),
            .rets_start = rOff,
            .rets_count = static_cast<uint32_t>(rets.size()),
            .protoId = protoId,
        });
        m_FuncIndex.emplace(name, sigIdx);
    }

    void SignatureTable::Freeze()
    {
        ZN_CORE_ASSERT(!m_Frozen, "SignatureTable already frozen");
        m_TPool.shrink_to_fit();
        m_Signs.shrink_to_fit();
        m_Frozen = true;
    }

    std::string_view TypeChecker::IdentName(uint32_t ident_node) const
    {
        ZN_CORE_ASSERT(m_Ast.nodes[ident_node].type == NodeType::Identifier,
                    "IdentName: expected Identifier node");
        return m_Ast.tokens[m_Ast.nodes[ident_node].start_token].literal;
    }

    CTType TypeChecker::ResolveTypeNode(uint32_t type_node)
    {
        if (type_node == ERROR_NODE) return CTType::Error;
        const Node& n = m_Ast.nodes[type_node];

        switch (n.type)
        {
            case NodeType::NamedType: 
            {
                const auto& nt = n.data.named_type;
                if (nt.args != ERROR_NODE) 
                {
                    AddError(type_node, DiagType::UnsupportedFeature,
                            "generic type arguments not supported in Phase 3");
                    return CTType::Error;
                }
                std::string_view name = IdentName(nt.ident);
                if (name == "i64") return CTType::I64;
                if (name == "f64") return CTType::F64;
                if (name == "bool") return CTType::Bool;
                if (name == "unit") return CTType::Unit;

                AddError(type_node, DiagType::UnknownTypeName,
                        "unknown type '" + std::string(name) + "'");
                return CTType::Error;
            }
            case NodeType::TupleType: 
            {
                const uint32_t count = n.data.list.count;
                if (count == 0) return CTType::Unit;
                if (count == 1) return ResolveTypeNode(ListItem(type_node, 0));

                AddError(type_node, DiagType::UnsupportedFeature,
                        "tuple types not supported in Phase 3");
                return CTType::Error;
            }
            case NodeType::RefType:
            {
                AddError(type_node, DiagType::UnsupportedFeature,
                        "reference types not supported in Phase 3");
                return CTType::Error;
            }
            case NodeType::DynType:
            {
                AddError(type_node, DiagType::UnsupportedFeature,
                        "dyn types not supported in Phase 3");
                return CTType::Error;
            }
            default:
            {
                AddError(type_node, DiagType::UnknownTypeName,
                        "unsupported type annotation");
                return CTType::Error;
            }
        }
    }

    uint32_t TypeChecker::CollectParams(std::vector<CTType>& buffer, uint32_t params_list_node)
    {
        const uint32_t count = ListCount(params_list_node);
        for (uint32_t i = 0; i < count; ++i) 
        {
            const uint32_t param_idx = ListItem(params_list_node, i);  
            const auto& ti = m_Ast.nodes[param_idx].data.typed_ident;

            if (ti.type == ERROR_NODE) 
            {
                AddError(param_idx, DiagType::MissingParamType,
                        "parameter requires type annotation in Phase 3");
                buffer.push_back(CTType::Error);
                continue;
            }
            buffer.push_back(ResolveTypeNode(ti.type));
        }
        return count;
    }

    uint32_t TypeChecker::CollectRet(std::vector<CTType>& buffer, uint32_t ret_type_node)
    {
        auto type = ResolveTypeNode(ret_type_node);
        if (type == CTType::Unit || type == CTType::Error) return 0;
        buffer.push_back(type);
        return 1;
    }

    void TypeChecker::CollectSignature()
    {
        ZN_CORE_ASSERT(m_Ast.root < m_Ast.nodes.size(), "invalid root index");
        ZN_CORE_ASSERT(m_Ast.nodes[m_Ast.root].type == NodeType::Program,
                    "AST root must be Program");

        const uint32_t root = m_Ast.root;
        const uint32_t child_count = m_Ast.nodes[root].data.list.count;
        const uint32_t child_start = m_Ast.nodes[root].data.list.start;

        std::vector<CTType> scratch;
        scratch.reserve(8);

        for (uint32_t i = 0; i < child_count; ++i) 
        {
            const uint32_t decl_idx = m_Ast.extra[child_start + i];
            const Node& decl = m_Ast.nodes[decl_idx];
            if (decl.type != NodeType::FuncDecl) continue;

            const uint32_t sign_idx  = decl.data.func_decl.sign;
            const Node& sign_node = m_Ast.nodes[sign_idx];
            ZN_CORE_ASSERT(sign_node.type == NodeType::FuncSign, "FuncDecl must point to FuncSign");

            const uint32_t name_idx = sign_node.data.func_sign.name;
            std::string_view name = IdentName(name_idx);

            if (m_Table.Find(name).has_value()) 
            {
                AddError(sign_idx, DiagType::DuplicateFunction,
                        "duplicate function '" + std::string(name) + "'");
                continue;
            }

            scratch.clear();

            const uint32_t p_count = CollectParams(scratch, sign_node.data.func_sign.params);
            const uint32_t r_count = CollectRet(scratch, sign_node.data.func_sign.ret_type);
            const uint16_t protoId = static_cast<uint16_t>(m_NextProtoId++);

            m_Table.Add(name,
                        { scratch.data(), p_count },
                        { scratch.data() + p_count, r_count },
                        protoId);
            if (name == "main") m_MainProtoId = protoId;
        }

        m_Table.Freeze();

        if (m_MainProtoId == UINT16_MAX) AddError(m_Ast.root, DiagType::MissingMainFunction,
                                                        "program must define 'func main'");
    }

    bool TypeChecker::Validate()
    {
        ZN_CORE_ASSERT(!m_Validated, "Validate() called twice");
        m_Validated = true;
        m_Errors.clear();

        CollectSignature();

        // TODO: add infer and check pass

        for (const auto& d : m_Errors) 
            if (d.severity == Severity::Error) return false;
        return true;
    }

    void TypeChecker::AddError(uint32_t node, DiagType type, std::string_view message)
    {
        ZN_CORE_ASSERT(node < m_Ast.nodes.size(), "AddError: invalid node index");
        const uint32_t token = m_Ast.nodes[node].start_token;
        ZN_CORE_ASSERT(token < m_Ast.tokens.size(), "AddError: invalid token index");

        m_Errors.push_back(Diagnostic
        {
            .message = std::string(message),
            .node = node,
            .token = token,
            .diag = type,
        });
    }
}