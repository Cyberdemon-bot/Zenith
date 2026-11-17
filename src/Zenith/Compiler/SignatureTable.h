#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Zenith/Compiler/CTType.h"

namespace Zenith {

    struct FuncSign
    {
        uint32_t params_start, params_count;
        CTType ret_type = CTType::Unit();
        uint16_t protoId;
    };

    struct FuncView
    {
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
}