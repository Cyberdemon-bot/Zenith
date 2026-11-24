#pragma once

#include <concepts>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Zenith/Compiler/Node.h"
#include "Zenith/Compiler/CTType.h"

namespace Zenith {

    struct FuncSign
    {
        std::string_view name;
        uint32_t decl_node = 0;
        uint32_t params_start, params_count;
        CTType ret_type = CTType::Unit();
        uint16_t protoId;
    };

    struct FuncView
    {
        std::string_view name;
        uint32_t decl_node = 0;
        std::span<const CTType> params;
        CTType ret_type = CTType::Unit();
        uint16_t protoId;
    };

    class SignatureTable
    {
    public:
        std::optional<FuncView> Find(std::string_view name) const;

        void Add(std::string_view name,
                 std::span<const CTType> params = {},
                 CTType ret_type = CTType::Unit(),
                 uint16_t protoId = 0,
                 uint32_t decl_node = ERROR_NODE);

        // Locks Add().
        // Any FuncView span obtained before Add() or Clear() may be invalidated.
        void Freeze();
        void Clear();

        size_t Count() const { return m_Signs.size(); }

        template<std::invocable<const FuncView&> Fn>
        void ForEach(Fn&& fn) const
        {
            for (const FuncSign& sig : m_Signs)
            {
                FuncView view
                {
                    .name = sig.name,
                    .decl_node = sig.decl_node,
                    .params = { m_TPool.data() + sig.params_start, sig.params_count },
                    .ret_type = sig.ret_type,
                    .protoId = sig.protoId,
                };
                fn(view);
            }
        }


    private:
        std::vector<CTType> m_TPool;
        std::vector<FuncSign> m_Signs;
        std::unordered_map<std::string_view, uint32_t> m_FuncIndex;

        bool m_Frozen = false;
    };
}