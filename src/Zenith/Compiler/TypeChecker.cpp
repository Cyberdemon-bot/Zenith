#include "Zenith/Core/Assert.h"
#include "Zenith/Compiler/Node.h"
#include "Zenith/Compiler/TypeChecker.h"
#include "Zenith/Compiler/Utils.h"

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

    void TypeChecker::PopTarget()
    {
        m_Targets.pop_back();
    }

    ControlTarget TypeChecker::GetCurrentTarget()
    {
        ZN_CORE_ASSERT(!m_Targets.empty(), "No Target to get!")
        return m_Targets.back();
    }

    ControlTarget* TypeChecker::FindBreak(uint32_t label)
    {
        if (m_Targets.empty()) return nullptr;
        if (label == ERROR_NODE) return &m_Targets.back();

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

    bool TypeChecker::Validate()
    {
        ZN_CORE_ASSERT(!m_Validated, "Validate() called twice");
        m_Validated = true;
        m_Errors.clear();

        m_NodeTypes.assign(m_Ast.nodes.size(), CTType::Error());
        CollectSignature();

        // TODO: add infer and check pass

        for (const auto& d : m_Errors) 
            if (d.severity == Severity::Error) return false;
        return true;
    }

    std::string_view TypeChecker::GetName(uint32_t node) const
    {
        ZN_CORE_ASSERT(m_Ast.nodes[node].type == NodeType::Identifier || m_Ast.nodes[node].type == NodeType::Label,
                    "GetName: expected Identifier or Label node");
        return m_Ast.tokens[m_Ast.nodes[node].start_token].literal;
    }

    CTType TypeChecker::ResolveBinaryOp(uint32_t node, Operator op, CTType lhs, CTType rhs)
    {
        if (lhs == CTType::Error() || rhs == CTType::Error()) return CTType::Error();
        if (lhs != rhs)
        {
            AddError(node, {DiagType::OperandTypeMismatch, "Operands of binary operator must have the same type"});
            return CTType::Error();
        }

        switch (op)
        {
            case Operator::Add:
            case Operator::Sub:
            case Operator::Mul:
            case Operator::Div:
            {
                if (!Utils::IsNumeric(lhs))
                {
                    AddError(node, {DiagType::OperandTypeMismatch, "Arithmetic operators require numeric operands"});
                    return CTType::Error();
                }
                return lhs;
            }
            case Operator::Mod:
            {
                if (!Utils::IsInteger(lhs))
                {
                    AddError(node, {DiagType::OperandTypeMismatch, "Operator '%' requires integer operands"});
                    return CTType::Error();
                }
                return lhs;
            }
            case Operator::Eq:
            case Operator::Ne: return CTType::Bool();
            case Operator::Lt:
            case Operator::Lte:
            case Operator::Gt:
            case Operator::Gte:
            {
                if (!Utils::IsNumeric(lhs))
                {
                    AddError(node, {DiagType::OperandTypeMismatch, "Ordered comparison requires numeric operands"});
                    return CTType::Error();
                }
                return CTType::Bool();
            }
            case Operator::And:
            case Operator::Or:
            {
                if (lhs != CTType::Bool())
                {
                    AddError(node, {DiagType::OperandTypeMismatch, "Logical operators require boolean operands"});
                    return CTType::Error();
                }
                return CTType::Bool();
            }
            default:
            {
                AddError(node, {DiagType::OperandTypeMismatch, "Unsupported binary operator"});
                return CTType::Error();
            }
        }
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
                    AddError(type_node, {DiagType::UnsupportedFeature,
                            "generic type arguments not supported in Phase 3"});
                    return CTType::Error();
                }
                std::string_view name = GetName(nt.ident);
                if (name == "i64") return CTType::I64();
                if (name == "f64") return CTType::F64();
                if (name == "bool") return CTType::Bool();
                if (name == "unit") return CTType::Unit();

                AddError(type_node, {DiagType::UnknownTypeName,
                        "unknown type '" + std::string(name) + "'"});
                return CTType::Error();
            }
            case NodeType::TupleType: 
            {
                // TODO: Update tuple support later
                AddError(type_node, {DiagType::UnsupportedFeature,
                        "tuple types not supported yet"});
                return CTType::Error();
            }
            case NodeType::RefType:
            {
                AddError(type_node, {DiagType::UnsupportedFeature,
                        "reference types not supported in Phase 3"});
                return CTType::Error();
            }
            case NodeType::DynType:
            {
                AddError(type_node, {DiagType::UnsupportedFeature,
                        "dyn types not supported in Phase 3"});
                return CTType::Error();
            }
            default:
            {
                AddError(type_node, {DiagType::UnknownTypeName,
                        "unsupported type annotation"});
                return CTType::Error();
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
                AddError(param_idx, {DiagType::MissingParamType,
                        "parameter requires type annotation in Phase 3"});
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
        ZN_CORE_ASSERT(m_Ast.root < m_Ast.nodes.size(), "invalid root index");
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
                AddError(sign_idx, {DiagType::DuplicateFunction,
                        "duplicate function '" + std::string(name) + "'"});
                continue;
            }

            m_Scratch.clear();

            const uint32_t p_count = CollectParams(m_Scratch, sign_node.data.func_sign.params);
            const CTType ret_type  = CollectRet(sign_node.data.func_sign.ret_type);
            const uint16_t protoId = static_cast<uint16_t>(m_NextProtoId++);

            m_Table.Add(name,
                        { m_Scratch.data(), p_count },
                        ret_type,
                        protoId);
            if (name == "main") m_MainProtoId = protoId;
        }

        m_Table.Freeze();

        if (m_MainProtoId == UINT16_MAX) AddError(m_Ast.root, {DiagType::MissingMainFunction,
                                                        "program must define 'func main'"});
    }

    void TypeChecker::AddError(uint32_t node, Error err)
    {
        ZN_CORE_ASSERT(node < m_Ast.nodes.size(), "AddError: invalid node index");
        const uint32_t token = m_Ast.nodes[node].start_token;
        ZN_CORE_ASSERT(token < m_Ast.tokens.size(), "AddError: invalid token index");

        m_Errors.push_back(Diagnostic
        {
            .message = std::string(err.message),
            .node = node,
            .token = token,
            .diag = err.type,
        });
    }
}