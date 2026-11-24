#pragma once

#include "Zenith/Core/Base.h"
#include "Zenith/VM/Instruction.h"
#include "Zenith/VM/Value.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Zenith {

    class Module;
    struct Proto
    {
        Proto() = default;

        uint32_t CodeOffset  = 0;
        uint32_t CodeCount   = 0;
        uint32_t ConstOffset = 0;
        uint32_t ConstCount  = 0;
        uint16_t FirstChild  = 0;
        uint16_t ChildCount  = 0;
        uint8_t  NumParams   = 0;
        uint8_t  MaxRegs     = 0;
        uint8_t  NumReturn   = 0;
    };

    class ProtoBuilder
    {
    public:
        ProtoBuilder() = default;

        uint16_t PushConstant(Value value);
        uint32_t CommitInstruction(Instruction instruction);

        void PatchJmp(uint32_t idx, int16_t sbx);
        void SetNumParams(uint8_t n) { m_NumParams = n; }
        void SetNumReturn(uint8_t n) { m_NumReturn = n; }
        void SetMaxRegs(uint8_t n) { m_MaxRegs = n; }

        uint8_t GetNumParams() const { return m_NumParams; }
        uint8_t GetNumReturn() const { return m_NumReturn; }
        uint8_t GetMaxRegs() const { return m_MaxRegs; }
        uint32_t GetCodeSize() const  { return static_cast<uint32_t>(m_Code.size()); }
        uint16_t GetConstSize() const { return static_cast<uint16_t>(m_Constants.size()); }

        void SetChildren(uint16_t first, uint16_t count)
        {
            m_FirstChild = first;
            m_ChildCount = count;
        }

        void Clear()
        {
            m_Code.clear(); m_Constants.clear();
            m_FirstChild = m_ChildCount = 0;
            m_NumParams = m_NumReturn = m_MaxRegs = 0;
        }
    private:
        std::vector<Instruction> m_Code;
        std::vector<Value> m_Constants;
        uint16_t m_FirstChild = 0, m_ChildCount = 0;
        uint8_t m_NumParams = 0, m_NumReturn = 0, m_MaxRegs = 0;

        friend class Module;
    };

    class Module
    {
    public:
        Module() = default;
        Module(Module&&) = default;
        Module& operator=(Module&&) = default;
        Module(const Module&) = delete;
        Module& operator=(const Module&) = delete;

        const Proto& GetProto(uint16_t id) const;
        uint16_t GetNumProtos() const { return static_cast<uint16_t>(m_Protos.size()); }
        std::span<const Instruction> GetCode(uint16_t id) const;
        std::span<const Value> GetConsts(uint16_t id) const;

        uint16_t Reserve(uint16_t count);
        uint16_t Reserve() { return Reserve(1); }
        uint16_t Add(const ProtoBuilder& b);
        void Fill(uint16_t id, const ProtoBuilder& b);

        void SetEntry(uint16_t id);
        uint16_t GetEntry() const { return m_EntryProto; }
        bool HasEntry() const { return m_HasEntry; }

        void Freeze();
        bool IsFrozen() const { return m_Frozen; }
        void Clear();
    private:
        std::vector<Proto> m_Protos;
        std::vector<Value> m_Constants;
        std::vector<Instruction> m_Code;
        uint16_t m_EntryProto = INVALID_PROTO;
        bool m_HasEntry = false;
        bool m_Frozen = false;
    };
}
