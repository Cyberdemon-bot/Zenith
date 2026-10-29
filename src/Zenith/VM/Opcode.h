#pragma once

#include <array>
#include <cstdint>

#include "Zenith/VM/Format.h"
#include "Zenith/VM/Value.h"

namespace Zenith {

    enum class Opcode : uint8_t 
    {
        LoadConst = 0,
        Add, Sub, Mul, Div, DivS, DivU, RemS, RemU, Neg,
        Halt,
        Count
    };

    struct OpcodeInfo 
    {
        Opcode  op;
        Format  format;
        uint8_t variants;
    };

    constexpr uint32_t OPCODE_COUNT = static_cast<uint32_t>(Opcode::Count);

    constexpr std::array<OpcodeInfo, OPCODE_COUNT> OPCODE_INFO = {{
        { Opcode::LoadConst, Format::iABx, 7 },
        { Opcode::Add,       Format::iABC, 6 },
        { Opcode::Sub,       Format::iABC, 6 },
        { Opcode::Mul,       Format::iABC, 6 },
        { Opcode::Div,       Format::iABC, 2 },
        { Opcode::DivS,      Format::iABC, 2 },
        { Opcode::DivU,      Format::iABC, 2 },
        { Opcode::RemS,      Format::iABC, 2 },
        { Opcode::RemU,      Format::iABC, 2 },
        { Opcode::Neg,       Format::iABC, 4 },
        { Opcode::Halt,      Format::iNone, 1 },
    }};

    constexpr bool opcode_table_is_consistent() 
    {
        for (uint32_t i = 0; i < OPCODE_COUNT; ++i) 
        {
            if (static_cast<uint32_t>(OPCODE_INFO[i].op) != i) return false;
            if (OPCODE_INFO[i].variants == 0) return false;
        }
        return true;
    }
    static_assert(opcode_table_is_consistent(), "OPCODE_INFO out of order or empty");

    constexpr auto OPCODE_BASE = []{
        std::array<uint32_t, OPCODE_COUNT + 1> b{};
        for (uint32_t i = 0; i < OPCODE_COUNT; ++i)
            b[i + 1] = b[i] + OPCODE_INFO[i].variants;
        return b;
    }();

    constexpr uint32_t DISPATCH_SIZE   = OPCODE_BASE[OPCODE_COUNT];
    constexpr uint32_t INVALID_HANDLER = DISPATCH_SIZE;
    static_assert(DISPATCH_SIZE <= 256, "dispatch exceeds byte opcode field");

    struct VariantList 
    {
        std::array<Type, DISPATCH_SIZE> data{};
        uint32_t size = 0;
        uint32_t op_start = 0;
        Opcode current = Opcode::Count;

        consteval VariantList op(Opcode o) const 
        {
            if (current == Opcode::Count && size > 0)
                throw "opcode list already sealed";
            if (current != Opcode::Count &&
                size - op_start != OPCODE_INFO[uint32_t(current)].variants)
                throw "variant count mismatch for previous opcode";
            if (size != OPCODE_BASE[uint32_t(o)])
                throw "opcode out of order";

            VariantList next = *this;
            next.current  = o;
            next.op_start = next.size;
            return next;
        }

        consteval VariantList add(Type t) const 
        {
            if (current == Opcode::Count)
                throw "add() called before slot())";
            VariantList next = *this;
            if (next.size - next.op_start >= OPCODE_INFO[uint32_t(next.current)].variants)
                throw "too many variants for current opcode";
            next.data[next.size] = t;
            next.size++;
            return next;
        }

        consteval bool is_sealed() const 
        {
            return current == Opcode::Count && size == DISPATCH_SIZE;
        }
    };

    constexpr VariantList OPCODE_VARIANTS_BUILD = VariantList{}
        .op(Opcode::LoadConst)
            .add(Type::U32).add(Type::I32).add(Type::F32).add(Type::U64)
            .add(Type::I64).add(Type::F64).add(Type::REF)
        .op(Opcode::Add)
            .add(Type::U32).add(Type::I32).add(Type::F32)
            .add(Type::U64).add(Type::I64).add(Type::F64)
        .op(Opcode::Sub)
            .add(Type::U32).add(Type::I32).add(Type::F32)
            .add(Type::U64).add(Type::I64).add(Type::F64)
        .op(Opcode::Mul)
            .add(Type::U32).add(Type::I32).add(Type::F32)
            .add(Type::U64).add(Type::I64).add(Type::F64)
        .op(Opcode::Div)
            .add(Type::F32)
            .add(Type::F64)
        .op(Opcode::DivS)
            .add(Type::I32)
            .add(Type::I64)
        .op(Opcode::DivU)
            .add(Type::U32)
            .add(Type::U64)
        .op(Opcode::RemS)
            .add(Type::I32)
            .add(Type::I64)
        .op(Opcode::RemU)
            .add(Type::U32)
            .add(Type::U64)
        .op(Opcode::Neg)
            .add(Type::I32).add(Type::I64)
            .add(Type::F32).add(Type::F64)
        .op(Opcode::Halt)
            .add(Type::None)
        .op(Opcode::Count);

    static_assert(OPCODE_VARIANTS_BUILD.is_sealed(),
                "OPCODE_VARIANTS must be sealed with .op(Opcode::Count)");

    constexpr std::array<Type, DISPATCH_SIZE> OPCODE_VARIANTS = OPCODE_VARIANTS_BUILD.data;

    constexpr bool no_duplicate_types() 
    {
        for (uint32_t i = 0; i < OPCODE_COUNT; ++i) 
        {
            uint64_t seen = 0;
            for (uint32_t v = 0; v < OPCODE_INFO[i].variants; ++v) 
            {
                uint32_t ty = static_cast<uint32_t>(OPCODE_VARIANTS[OPCODE_BASE[i] + v]);
                if (seen & (1ull << ty)) return false;
                seen |= (1ull << ty);
            }
        }
        return true;
    }
    static_assert(no_duplicate_types(), "duplicate type within one opcode's variant list");

    constexpr auto OPCODE_DISPATCH = []{
        std::array<std::array<uint32_t, TYPE_COUNT>, OPCODE_COUNT> t{};
        for (auto& row : t) for (auto& v : row) v = INVALID_HANDLER;
        for (uint32_t i = 0; i < OPCODE_COUNT; ++i)
            for (uint32_t v = 0; v < OPCODE_INFO[i].variants; ++v) 
            {
                uint32_t slot = OPCODE_BASE[i] + v;
                t[i][static_cast<uint32_t>(OPCODE_VARIANTS[slot])] = slot;
            }
        return t;
    }();

    struct SlotInfo 
    { 
        Type ty; 
        Opcode op; 
        Format format; 
    };

    constexpr std::array<SlotInfo, DISPATCH_SIZE> SLOT_INFO = []{
        std::array<SlotInfo, DISPATCH_SIZE> s{};
        for (uint32_t i = 0; i < OPCODE_COUNT; ++i)
            for (uint32_t v = 0; v < OPCODE_INFO[i].variants; ++v) 
            {
                uint32_t slot = OPCODE_BASE[i] + v;
                s[slot] = { OPCODE_VARIANTS[slot], OPCODE_INFO[i].op, OPCODE_INFO[i].format };
            }
        return s;
    }();
} 