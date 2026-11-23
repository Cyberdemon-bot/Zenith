#include "Zenith/Core/Assert.h"
#include "Zenith/Compiler/SignatureTable.h"

namespace Zenith {

    std::optional<FuncView> SignatureTable::Find(std::string_view name) const
    {
        auto it = m_FuncIndex.find(name);
        if (it == m_FuncIndex.end()) return std::nullopt;

        const FuncSign& sig = m_Signs[it->second];
        return FuncView
        {
            .params = { m_TPool.data() + sig.params_start, sig.params_count },
            .ret_type = sig.ret_type,
            .protoId = sig.protoId,
        };
    }

    void SignatureTable::Add(std::string_view name,
                            std::span<const CTType> params,
                            CTType ret_type,
                            uint16_t protoId)
    {
        ZN_CORE_ASSERT(!m_Frozen, "Cannot add signature: table is frozen");
        ZN_CORE_ASSERT(m_FuncIndex.find(name) == m_FuncIndex.end(),
                    "duplicate function signature added");

        const uint32_t pOff = static_cast<uint32_t>(m_TPool.size());
        m_TPool.insert(m_TPool.end(), params.begin(), params.end());

        const uint32_t sigIdx = static_cast<uint32_t>(m_Signs.size());
        m_Signs.push_back(FuncSign
        {
            .params_start = pOff,
            .params_count = static_cast<uint32_t>(params.size()),
            .ret_type = ret_type,
            .protoId = protoId,
        });
        m_FuncIndex.emplace(name, sigIdx);
    }

    void SignatureTable::Freeze()
    {
        ZN_CORE_ASSERT(!m_Frozen, "SignatureTable already frozen");
        m_Frozen = true;
    }

    void SignatureTable::Clear()
    {
        m_TPool.clear();
        m_Signs.clear();
        m_FuncIndex.clear();
        m_Frozen = false;
    }
}
