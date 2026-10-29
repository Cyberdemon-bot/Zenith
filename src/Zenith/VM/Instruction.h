#pragma once

#include <cstdint>
#include <tuple>
#include <bit>
#include <type_traits>

#include "Zenith/Core/Assert.h"
#include "Zenith/VM/Format.h"
#include "Zenith/VM/Opcode.h"
#include "Zenith/VM/Value.h"

namespace Zenith {

    namespace detail {
        template<typename...> struct first_or_void { using type = void; };
        template<typename T, typename... Rest>
        struct first_or_void<T, Rest...> { using type = T; };

        template<typename... Args>
        inline constexpr bool starts_with_type =
            std::is_same_v<typename first_or_void<Args...>::type, Type>;
    }

    struct Instruction 
    {
        static constexpr uint32_t INVALID_RAW = 0xFFFFFFFFu;
        uint32_t raw = INVALID_RAW;

        Instruction() = default;

        template<Format F, typename... Args>
        static constexpr Instruction make(Opcode op, Args... args) noexcept 
        {
            Instruction inst;
            const uint32_t oi = static_cast<uint32_t>(op);
            ZN_CORE_ASSERT(oi < OPCODE_COUNT, "Opcode out of range");
            ZN_CORE_ASSERT(OPCODE_INFO[oi].format == F, "Format mismatch for opcode");

            if constexpr (sizeof...(Args) == 0) 
            {
                ZN_CORE_ASSERT(OPCODE_INFO[oi].variants == 1,
                               "Untyped opcode required");
                inst.raw = OPCODE_BASE[oi] << 24;
            } 
            else if constexpr (detail::starts_with_type<Args...>) 
            {
                auto full = std::tuple{args...};
                const Type ty = std::get<0>(full);
                const uint32_t slot = OPCODE_DISPATCH[oi][static_cast<uint32_t>(ty)];
                ZN_CORE_ASSERT(slot != INVALID_HANDLER,
                               "Type not supported by this opcode");
                auto operands = std::apply(
                    [](auto&&, auto&&... rest) { return std::tuple{rest...}; }, full);
                inst.raw = pack<F>(slot, operands,
                            std::make_index_sequence<sizeof...(Args) - 1>{});
            } 
            else 
            {
                ZN_CORE_ASSERT(OPCODE_INFO[oi].variants == 1,
                               "Untyped opcode required");
                ZN_CORE_ASSERT(OPCODE_VARIANTS[OPCODE_BASE[oi]] == Type::None,
                               "Untyped opcode must have Type::None variant");
                inst.raw = pack<F>(OPCODE_BASE[oi], std::tuple{args...},
                            std::make_index_sequence<sizeof...(Args)>{});
            }
            return inst;
        }

        constexpr void patch_sBx(int16_t sbx) noexcept 
        {
            raw = (raw & 0xFFFF0000u) | static_cast<uint32_t>(std::bit_cast<uint16_t>(sbx));
        }

        constexpr uint8_t slot() const noexcept { return uint8_t(raw >> 24); }

        template<Format F>
        auto extract() const noexcept 
        {
            if constexpr (F == Format::iNone) return std::tuple<>{};
            else if constexpr (F == Format::iABC)
                return std::tuple{ uint8_t(raw >> 16), uint8_t(raw >> 8), uint8_t(raw) };
            else if constexpr (F == Format::iABx)
                return std::tuple{ uint8_t(raw >> 16), uint16_t(raw) };
            else if constexpr (F == Format::iAsBx)
                return std::tuple{ uint8_t(raw >> 16),
                                std::bit_cast<int16_t>(uint16_t(raw)) };
            else if constexpr (F == Format::iAx)
                return std::tuple{ uint32_t(raw & 0xFFFFFFu) };
        }

    private:
        template<Format F, typename Tuple, size_t... Is>
        static constexpr uint32_t pack(uint32_t slot, const Tuple& t,
                                    std::index_sequence<Is...>) noexcept 
        {
            uint32_t r = slot << 24;
            if constexpr (F == Format::iNone) static_assert(sizeof...(Is) == 0, "iNone has no operands");
            else if constexpr (F == Format::iABC) 
            {
                static_assert(sizeof...(Is) == 3, "iABC requires (a, b, c)");
                r |= (uint32_t(static_cast<uint8_t>(std::get<0>(t))) << 16)
                | (uint32_t(static_cast<uint8_t>(std::get<1>(t))) << 8)
                |  uint32_t(static_cast<uint8_t>(std::get<2>(t)));
            }
            else if constexpr (F == Format::iABx) 
            {
                static_assert(sizeof...(Is) == 2, "iABx requires (a, Bx)");
                r |= (uint32_t(static_cast<uint8_t>(std::get<0>(t))) << 16)
                |  uint32_t(static_cast<uint16_t>(std::get<1>(t)));
            } 
            else if constexpr (F == Format::iAsBx) 
            {
                static_assert(sizeof...(Is) == 2, "iAsBx requires (a, sBx)");
                r |= (uint32_t(static_cast<uint8_t>(std::get<0>(t))) << 16)
                |  uint32_t(static_cast<uint16_t>(std::get<1>(t)));
            } 
            else if constexpr (F == Format::iAx) 
            {
                static_assert(sizeof...(Is) == 1, "iAx requires (Ax)");
                r |= uint32_t(static_cast<uint32_t>(std::get<0>(t))) & 0xFFFFFFu;
            }
            return r;
        }
    };

}