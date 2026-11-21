#pragma once

#include "Zenith/VM/Format.h"
#include "Zenith/VM/Value.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace Zenith {

    enum class Opcode : uint8_t 
    {
        LoadConst = 0,
        LoadImm, Mov,     
        Add, Sub, Mul, Div, 
        DivS, DivU, RemS, RemU, Neg,
        Eq, Lt, LNot, Cast,
        Jmp, Jz, Jnz,  
        Call, Return,
        Halt, Count
    };

    enum class CastKind : uint8_t
    {
        TruncI8 = 0,
        TruncI16,
        TruncU8,
        TruncU16,
        I32ToI64,
        I64ToI32,
        I32ToF32,
        I32ToF64,
        U32ToF32,
        U32ToF64,
        I64ToF32,
        I64ToF64,
        U64ToF32,
        U64ToF64,
        F32ToI32,
        F32ToU32,
        F32ToI64,
        F32ToU64,
        F64ToI32,
        F64ToU32,
        F64ToI64,
        F64ToU64,
        F32ToF64,
        F64ToF32,
        ToBool,
        Count
    };

    inline constexpr uint32_t OPCODE_COUNT = static_cast<uint32_t>(Opcode::Count);
    inline constexpr uint32_t MAX_VARIANTS = 8;   
    struct OpDef 
    {
        Opcode op = Opcode::Count;  
        Format format = Format::iNone;
        uint8_t variant_count = 0;
        std::array<Type, MAX_VARIANTS> variants{};

        constexpr OpDef() = default;

        template<typename... Ts>
        constexpr OpDef(Opcode o, Format f, Ts... ts)
            : op(o), format(f)
            , variant_count(static_cast<uint8_t>(sizeof...(Ts)))
            , variants{ static_cast<Type>(ts)... }
        {
            static_assert(sizeof...(Ts) >= 1, "opcode must have >=1 variant");
            static_assert(sizeof...(Ts) <= MAX_VARIANTS, "opcode exceeds MAX_VARIANTS");
            static_assert((std::is_same_v<Ts, Type> && ...), "variant args must be Type");
        }
    };

    inline constexpr std::array<OpDef, OPCODE_COUNT> OPCODE_DEFS = {{
         { Opcode::LoadConst, Format::iABx,  Type::None },
         { Opcode::LoadImm,   Format::iAsBx, Type::None },
         { Opcode::Mov,       Format::iABC,  Type::None },

         { Opcode::Add,       Format::iABC,  Type::U32, Type::U64, Type::F32, Type::F64 },
         { Opcode::Sub,       Format::iABC,  Type::U32, Type::U64, Type::F32, Type::F64 },
         { Opcode::Mul,       Format::iABC,  Type::U32, Type::U64, Type::F32, Type::F64 },
         { Opcode::Div,       Format::iABC,  Type::F32, Type::F64 },
         { Opcode::DivS,      Format::iABC,  Type::I32, Type::I64 },
         { Opcode::DivU,      Format::iABC,  Type::U32, Type::U64 },
         { Opcode::RemS,      Format::iABC,  Type::I32, Type::I64 },
        { Opcode::RemU,      Format::iABC,  Type::U32, Type::U64 },
        { Opcode::Neg,       Format::iABC,  Type::I32, Type::I64, Type::F32, Type::F64 },

        { Opcode::Eq,        Format::iABC,  Type::U32, Type::U64, Type::F32, Type::F64 },
        { Opcode::Lt,        Format::iABC,  Type::U32, Type::I32, Type::F32, Type::U64, Type::I64, Type::F64 },
        { Opcode::LNot,      Format::iABC,  Type::None },
        { Opcode::Cast,      Format::iABC,  Type::None },
        { Opcode::Jmp,       Format::iAsBx, Type::None },
        { Opcode::Jz,        Format::iAsBx, Type::None },
        { Opcode::Jnz,       Format::iAsBx, Type::None },

        { Opcode::Call,      Format::iABx,  Type::None },
        { Opcode::Return,    Format::iABC,  Type::None },
        { Opcode::Halt,      Format::iNone, Type::None },
    }};

    constexpr bool OpcodeDefsOrdered() 
    {
        for (uint32_t i = 0; i < OPCODE_COUNT; ++i)
            if (static_cast<uint32_t>(OPCODE_DEFS[i].op) != i) return false;
        return true;
    }
    static_assert(OpcodeDefsOrdered(), "OPCODE_DEFS must follow Opcode enum order exactly");

    constexpr bool AllHaveVariants() 
    {
        for (auto& d : OPCODE_DEFS) if (d.variant_count == 0) return false;
        return true;
    }
    static_assert(AllHaveVariants(), "every opcode must declare >=1 variant");

    constexpr bool NoDuplicateTypes() 
    {
        for (auto& d : OPCODE_DEFS) 
        {
            uint64_t seen = 0;
            for (uint32_t v = 0; v < d.variant_count; ++v) 
            {
                uint32_t ty = static_cast<uint32_t>(d.variants[v]);
                if (seen & (1ull << ty)) return false;
                seen |= (1ull << ty);
            }
        }
        return true;
    }
    static_assert(NoDuplicateTypes(), "duplicate type within one opcode's variant list");

    struct OpcodeInfo 
    {
        Opcode op;
        Format format;
        uint8_t variants;
    };

    inline constexpr auto OPCODE_INFO = []{
        std::array<OpcodeInfo, OPCODE_COUNT> info{};
        for (uint32_t i = 0; i < OPCODE_COUNT; ++i)
            info[i] = { OPCODE_DEFS[i].op, OPCODE_DEFS[i].format, OPCODE_DEFS[i].variant_count };
        return info;
    }();

    inline constexpr auto OPCODE_BASE = []{
        std::array<uint32_t, OPCODE_COUNT + 1> b{};
        for (uint32_t i = 0; i < OPCODE_COUNT; ++i)
            b[i + 1] = b[i] + OPCODE_DEFS[i].variant_count;
        return b;
    }();

    inline constexpr uint32_t DISPATCH_SIZE   = OPCODE_BASE[OPCODE_COUNT];
    inline constexpr uint32_t INVALID_HANDLER = DISPATCH_SIZE;
    static_assert(DISPATCH_SIZE <= 256, "dispatch exceeds byte opcode field");

    inline constexpr auto OPCODE_VARIANTS = []{
        std::array<Type, DISPATCH_SIZE> v{};
        for (uint32_t i = 0; i < OPCODE_COUNT; ++i)
            for (uint32_t j = 0; j < OPCODE_DEFS[i].variant_count; ++j)
                v[OPCODE_BASE[i] + j] = OPCODE_DEFS[i].variants[j];
        return v;
    }();
    struct SlotAlias 
    {
        Opcode op;
        Type from;
        Type to;
    };

    inline constexpr auto SLOT_ALIASES = std::to_array<SlotAlias>({
        { Opcode::Add, Type::I32, Type::U32 },
        { Opcode::Add, Type::I64, Type::U64 },
        { Opcode::Sub, Type::I32, Type::U32 },
        { Opcode::Sub, Type::I64, Type::U64 },
        { Opcode::Mul, Type::I32, Type::U32 },
        { Opcode::Mul, Type::I64, Type::U64 },
        { Opcode::Eq,  Type::I32, Type::U32 },
        { Opcode::Eq,  Type::I64, Type::U64 },
        { Opcode::Eq,  Type::REF, Type::U64 },
    });

    constexpr bool AliasesAreValid() 
    {
        for (const auto& a : SLOT_ALIASES) 
        {
            const OpDef& d = OPCODE_DEFS[static_cast<uint32_t>(a.op)];
            bool found = false;
            for (uint32_t v = 0; v < d.variant_count; ++v)
                if (d.variants[v] == a.to) { found = true; break; }
            if (!found) return false;
        }
        return true;
    }
    static_assert(AliasesAreValid(), "alias target not present in OPCODE_DEFS");

    constexpr bool AliasesNoConflict() 
    {
        for (const auto& a : SLOT_ALIASES) 
        {
            const OpDef& d = OPCODE_DEFS[static_cast<uint32_t>(a.op)];
            for (uint32_t v = 0; v < d.variant_count; ++v)
                if (d.variants[v] == a.from) return false;
        }
        return true;
    }
    static_assert(AliasesNoConflict(), "alias source is also a real variant");

    constexpr bool AliasesNoDupSource() 
    {
        for (std::size_t i = 0; i < SLOT_ALIASES.size(); ++i)
            for (std::size_t j = i + 1; j < SLOT_ALIASES.size(); ++j)
                if (SLOT_ALIASES[i].op == SLOT_ALIASES[j].op &&
                    SLOT_ALIASES[i].from == SLOT_ALIASES[j].from)
                    return false;
        return true;
    }
    static_assert(AliasesNoDupSource(), "duplicate (op, from) pair in SLOT_ALIASES");

    inline constexpr auto OPCODE_DISPATCH = []{
        std::array<std::array<uint32_t, TYPE_COUNT>, OPCODE_COUNT> t{};
        for (auto& row : t) for (auto& v : row) v = INVALID_HANDLER;
        for (uint32_t i = 0; i < OPCODE_COUNT; ++i)
            for (uint32_t v = 0; v < OPCODE_DEFS[i].variant_count; ++v) 
            {
                uint32_t slot = OPCODE_BASE[i] + v;
                t[i][static_cast<uint32_t>(OPCODE_VARIANTS[slot])] = slot;
            }
        for (const auto& a : SLOT_ALIASES)
            t[static_cast<uint32_t>(a.op)][static_cast<uint32_t>(a.from)] =
                t[static_cast<uint32_t>(a.op)][static_cast<uint32_t>(a.to)];
        return t;
    }();

    struct SlotInfo 
    { 
        Type ty; 
        Opcode op; 
        Format format; 
    };

    inline constexpr std::array<SlotInfo, DISPATCH_SIZE> SLOT_INFO = []{
        std::array<SlotInfo, DISPATCH_SIZE> s{};
        for (uint32_t i = 0; i < OPCODE_COUNT; ++i)
            for (uint32_t v = 0; v < OPCODE_DEFS[i].variant_count; ++v) 
            {
                uint32_t slot = OPCODE_BASE[i] + v;
                s[slot] = { OPCODE_VARIANTS[slot], OPCODE_DEFS[i].op, OPCODE_DEFS[i].format };
            }
        return s;
    }();
}