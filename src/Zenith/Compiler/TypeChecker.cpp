#include "Zenith/Core/Assert.h"
#include "Zenith/Compiler/Node.h"
#include "Zenith/Compiler/Utils.h"
#include "Zenith/Compiler/TypeChecker.h"

#include <optional>
#include <ranges>

namespace Zenith {

    CTType TypeChecker::SetNodeType(uint32_t node, CTType type)
    {
        if (node == ERROR_NODE || node >= m_Ast.nodes.size()) return CTType::Error();
        m_NodeTypes[node] = type;
        return type;
    }

    void TypeChecker::PushScope()
    {
        ++m_CurrentDepth;
    }

    void TypeChecker::PopScope()
    {
        ZN_CORE_ASSERT(m_CurrentDepth > 0, "Scope underflow");
        while (!m_Symbols.empty() && m_Symbols.back().depth == m_CurrentDepth)
            m_Symbols.pop_back();
        --m_CurrentDepth;
    }

    bool TypeChecker::AddSymbol(const Symbol& sb)
    {
        for (const auto& symbol : m_Symbols | std::views::reverse)
        {
            if (symbol.depth != m_CurrentDepth) break;
            if (symbol.name == sb.name) return false;
        }

        m_Symbols.push_back(sb);
        return true;
    }

    std::optional<Symbol> TypeChecker::FindSymbols(std::string_view name)
    {
        for (const auto& symbol : m_Symbols | std::views::reverse)
            if (symbol.name == name) return symbol;
        return std::nullopt;
    }

    void TypeChecker::PushTarget(const ControlTarget& target)
    {
        m_Targets.push_back(target);
    }

    ControlTarget TypeChecker::PopTarget()
    {
        ZN_CORE_ASSERT(!m_Targets.empty(), "No target to pop");
        ControlTarget top = m_Targets.back();
        m_Targets.pop_back();
        return top;
    }

    ControlTarget* TypeChecker::FindBreak(uint32_t label)
    {
        if (m_Targets.empty()) return nullptr;
        if (label == ERROR_NODE)
        {
            for (auto& target : m_Targets | std::views::reverse)
                if (target.is_loop) return &target;
            return nullptr;
        }

        std::string_view name = GetName(label);
        for (auto& target : m_Targets | std::views::reverse)
            if (target.label == name) return &target;
        return nullptr;
    }

    ControlTarget* TypeChecker::FindContinue(uint32_t label)
    {
        if (m_Targets.empty()) return nullptr;

        std::string_view name = (label != ERROR_NODE) ? GetName(label) : "";
        for (auto& target : m_Targets | std::views::reverse)
            if (target.is_loop)
                if (label == ERROR_NODE || target.label == name)
                    return &target;
        return nullptr;
    }


    void TypeChecker::Clear()
    {
        m_Ast = {};
        m_Table.Clear();
        m_Solver.Reset();
        m_CurrentDepth = 0;
        m_NextProtoId = 0;
        m_MainProtoId = INVALID_PROTO;
        m_CurrentFunc = {};
        m_Errors.Clear();
        m_Symbols.clear();
        m_Targets.clear();
        m_Scratch.clear();
        m_NodeTypes.clear();
    }

    CTType TypeChecker::ResolveTypeNode(uint32_t type_node)
    {
        if (type_node == ERROR_NODE) return CTType::Error();
        const Node& n = m_Ast.nodes[type_node];

        switch (n.type)
        {
            case NodeType::NamedType:
            {
                const auto& nt = n.data.named_type;
                if (nt.args != ERROR_NODE)
                {
                    AddError(type_node, { DiagType::UnsupportedFeature, "generic type arguments" });
                    return SetNodeType(type_node, CTType::Error());
                }
                ZN_CORE_ASSERT(nt.ident < m_Ast.tokens.size(), "Invalid token index for NamedType");
                std::string_view name = m_Ast.tokens[nt.ident].literal;
                if (name == "i8")   return SetNodeType(type_node, CTType::I8());
                if (name == "i16")  return SetNodeType(type_node, CTType::I16());
                if (name == "i32")  return SetNodeType(type_node, CTType::I32());
                if (name == "i64")  return SetNodeType(type_node, CTType::I64());
                if (name == "u8")   return SetNodeType(type_node, CTType::U8());
                if (name == "u16")  return SetNodeType(type_node, CTType::U16());
                if (name == "u32")  return SetNodeType(type_node, CTType::U32());
                if (name == "u64")  return SetNodeType(type_node, CTType::U64());
                if (name == "f32")  return SetNodeType(type_node, CTType::F32());
                if (name == "f64")  return SetNodeType(type_node, CTType::F64());
                if (name == "bool") return SetNodeType(type_node, CTType::Bool());

                AddError(type_node, { DiagType::UnknownTypeName, name });
                return SetNodeType(type_node, CTType::Error());
            }
            // Phase 4+: composite and reference types
            case NodeType::TupleType:
            {
                if (ListCount(type_node) == 0) return SetNodeType(type_node, CTType::Unit());
                AddError(type_node, { DiagType::UnsupportedFeature, "tuple types" });
                return SetNodeType(type_node, CTType::Error());
            }
            case NodeType::RefType:
            {
                AddError(type_node, { DiagType::UnsupportedFeature, "reference types" });
                return SetNodeType(type_node, CTType::Error());
            }
            case NodeType::DynType:
            {
                AddError(type_node, { DiagType::UnsupportedFeature, "dyn types" });
                return SetNodeType(type_node, CTType::Error());
            }
            default:
            {
                AddError(type_node, { DiagType::UnsupportedFeature, "type annotation" });
                return SetNodeType(type_node, CTType::Error());
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
                AddError(param_idx, { DiagType::UnsupportedFeature, "untyped parameters" });
                SetNodeType(param_idx, CTType::Error());
                buffer.push_back(CTType::Error());
                continue;
            }
            buffer.push_back(ResolveTypeNode(ti.type));
        }
        return count;
    }

    CTType TypeChecker::CollectRet(uint32_t ret_type_node)
    {
        if (ret_type_node == ERROR_NODE) return CTType::Unit();
        return ResolveTypeNode(ret_type_node);
    }

    void TypeChecker::CollectSignature()
    {
        ZN_CORE_ASSERT(m_Ast.root < m_Ast.nodes.size(), "Invalid root index");
        ZN_CORE_ASSERT(m_Ast.nodes[m_Ast.root].type == NodeType::Program,
                    "AST root must be Program");

        const uint32_t root = m_Ast.root;
        const uint32_t child_count = ListCount(root);

        for (uint32_t i = 0; i < child_count; ++i)
        {
            const uint32_t decl_idx = ListItem(root, i);
            const Node& decl = m_Ast.nodes[decl_idx];
            if (decl.type != NodeType::FuncDecl) continue;

            const uint32_t sign_idx  = decl.data.func_decl.sign;
            const Node& sign_node = m_Ast.nodes[sign_idx];
            ZN_CORE_ASSERT(sign_node.type == NodeType::FuncSign, "FuncDecl must point to FuncSign");

            const uint32_t name_idx = sign_node.data.func_sign.name;
            std::string_view name = GetName(name_idx);

            if (m_Table.Find(name).has_value())
            {
                AddError(sign_idx, { DiagType::DuplicateFunction, name });
                SetNodeType(decl_idx, CTType::Error());
                SetNodeType(sign_idx, CTType::Error());
                continue;
            }

            m_Scratch.clear();

            const uint32_t p_count = CollectParams(m_Scratch, sign_node.data.func_sign.params);
            const CTType ret_type  = CollectRet(sign_node.data.func_sign.ret_type);
            ZN_CORE_ASSERT(m_NextProtoId < INVALID_PROTO, "Proto id overflow");
            const uint16_t protoId = static_cast<uint16_t>(m_NextProtoId++);

            SetNodeType(sign_idx, ret_type);
            SetNodeType(name_idx, ret_type);
            SetNodeType(decl_idx, ret_type);

            m_Table.Add(name,
                        { m_Scratch.data(), p_count },
                        ret_type,
                        protoId,
                        decl_idx);
            if (name == "main")
            {
                m_MainProtoId = protoId;
                if (p_count != 0 || (ret_type != CTType::Unit() && ret_type != CTType::I64()))
                    AddError(sign_idx, { DiagType::InvalidMainSignature });
            }
        }

        m_Table.Freeze();

        if (m_MainProtoId == INVALID_PROTO)
            AddError(m_Ast.root, { DiagType::MissingMainFunction });
    }

    void TypeChecker::AddError(uint32_t node, Error err)
    {
        ZN_CORE_ASSERT(node < m_Ast.nodes.size(), "Invalid node index");
        const uint32_t token = m_Ast.nodes[node].start_token;
        ZN_CORE_ASSERT(token < m_Ast.tokens.size(), "Invalid token index");

        m_Errors.Push(Diagnostic
        {
            .args = std::string(err.args),
            .node = node,
            .token = token,
            .expected = err.expected,
            .actual = err.actual,
            .severity = GetDiagDef(err.type).severity,
            .diag = err.type,
        });
    }

    std::string_view TypeChecker::GetName(uint32_t node) const
    {
        std::string_view name;
        [[maybe_unused]] bool ok = Utils::GetName(m_Ast, node, name);
        ZN_CORE_ASSERT(ok, "Expected Identifier or Label node");
        return name;
    }

    uint32_t TypeChecker::ListCount(uint32_t list_node) const
    {
        uint32_t count = 0;
        [[maybe_unused]] bool ok = Utils::ListCount(m_Ast, list_node, count);
        ZN_CORE_ASSERT(ok, "Invalid list node");
        return count;
    }

    uint32_t TypeChecker::ListItem(uint32_t list_node, uint32_t i) const
    {
        uint32_t item = 0;
        [[maybe_unused]] bool ok = Utils::ListItem(m_Ast, list_node, i, item);
        ZN_CORE_ASSERT(ok, "Invalid list node or index out of bounds");
        return item;
    }
}
