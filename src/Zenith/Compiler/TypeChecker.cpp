#include "Zenith/Compiler/Node.h"
#include "Zenith/Core/Assert.h"
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
        // insert data buffer
        const uint32_t pOff = static_cast<uint32_t>(m_TPool.size());
        m_TPool.insert(m_TPool.end(), params.begin(), params.end());
        const uint32_t rOff = static_cast<uint32_t>(m_TPool.size());
        m_TPool.insert(m_TPool.end(), rets.begin(), rets.end());

        // insert registry
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
        ZN_ASSERT(!m_Frozen, "SignatureTable already frozen");
        m_TPool.shrink_to_fit();
        m_Signs.shrink_to_fit();
        m_Frozen = true;
    }

    void TypeChecker::CollectSignature()
    {
        ZN_CORE_ASSERT(m_Ast.root < m_Ast.nodes.size(), "Invalid root index");
        ZN_CORE_ASSERT(m_Ast.nodes[m_Ast.root].type == NodeType::Program, "Ast's root should be program node");
        auto root = m_Ast.nodes[m_Ast.root];
        std::span<const uint32_t> program_child = std::span(m_Ast.extra.data() + root.data.list.start, root.data.list.count);

        for (uint32_t elem : program_child)
        {
            auto& delc = m_Ast.nodes[elem];
            if (delc.type != NodeType::FuncDecl) continue;

            auto& sign = m_Ast.nodes[delc.data.func_decl.sign];
            auto& name_node = m_Ast.nodes[sign.data.func_sign.name];
            std::string_view name = m_Ast.tokens[name_node.start_token].literal;

        }
    }
}