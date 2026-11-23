#include "Zenith/VM/Proto.h"

#include "Zenith/Core/Assert.h"
#include "Zenith/Core/Base.h"

#include <utility>

namespace Zenith {

    uint16_t ProtoBuilder::PushConstant(Value value) 
    {
        ZN_CORE_ASSERT(m_Constants.size() <= MAX_Bx, "Constant pool overflow (max 65536)");
        uint16_t idx = static_cast<uint16_t>(m_Constants.size());
        m_Constants.push_back(value);
        return idx;
    }

    uint32_t ProtoBuilder::CommitInstruction(Instruction instruction)
    {
        uint32_t idx = static_cast<uint32_t>(m_Code.size());
        m_Code.push_back(instruction);
        return idx;
    }

    void ProtoBuilder::PatchJmp(uint32_t idx, int16_t address)
    {
        ZN_CORE_ASSERT(idx < m_Code.size(), "Instruction index out of bounds");
        m_Code[idx].PatchSBx(address);
    }

    const Proto& Module::GetProto(uint16_t id) const 
    {
        ZN_CORE_ASSERT(id < m_Protos.size(), "Invalid proto id");
        return m_Protos[id];
    }

    std::span<const Instruction> Module::GetCode(uint16_t id) const 
    {
        ZN_CORE_ASSERT(id < m_Protos.size(), "Invalid proto id");
        const Proto& p = m_Protos[id];
        return { m_Code.data() + p.CodeOffset, p.CodeCount };
    }

    std::span<const Value> Module::GetConsts(uint16_t id) const 
    {
        ZN_CORE_ASSERT(id < m_Protos.size(), "Invalid proto id");
        const Proto& p = m_Protos[id];
        return { m_Constants.data() + p.ConstOffset, p.ConstCount };
    }

    uint16_t Module::Reserve() 
    {
        ZN_CORE_ASSERT(!m_Frozen, "Module is frozen");
        ZN_CORE_ASSERT(m_Protos.size() < INVALID_PROTO, "Proto table overflow");
        uint16_t id = static_cast<uint16_t>(m_Protos.size());
        m_Protos.emplace_back();
        return id;
    }

    uint16_t Module::Add(ProtoBuilder&& b) 
    {
        uint16_t id = Reserve();
        Fill(id, std::move(b));
        return id;
    }

    void Module::Fill(uint16_t id, ProtoBuilder&& b) 
    {
        ZN_CORE_ASSERT(!m_Frozen, "Module is frozen");
        ZN_CORE_ASSERT(id < m_Protos.size(), "Invalid reserved id");
        Proto& p = m_Protos[id];
        ZN_CORE_ASSERT(p.CodeCount == 0, "Proto already filled");

        p.CodeOffset = static_cast<uint32_t>(m_Code.size());
        p.CodeCount = static_cast<uint32_t>(b.m_Code.size());
        p.ConstOffset = static_cast<uint32_t>(m_Constants.size());
        p.ConstCount = static_cast<uint32_t>(b.m_Constants.size());
        p.FirstChild = b.m_FirstChild;
        p.ChildCount = b.m_ChildCount;
        p.NumParams = b.m_NumParams;
        p.NumReturn = b.m_NumReturn;
        p.MaxRegs = b.m_MaxRegs;

        m_Code.insert(m_Code.end(), b.m_Code.begin(), b.m_Code.end());
        m_Constants.insert(m_Constants.end(), b.m_Constants.begin(), b.m_Constants.end());
    }

    void Module::SetEntry(uint16_t id)
    {
        ZN_CORE_ASSERT(!m_Frozen, "Module is frozen");
        ZN_CORE_ASSERT(!m_HasEntry, "Entry proto already set");
        ZN_CORE_ASSERT(id < m_Protos.size(), "Invalid entry proto id");
        m_EntryProto = id;
        m_HasEntry = true;
    }

    void Module::Freeze() 
    {
        ZN_CORE_ASSERT(!m_Frozen, "Already frozen");
        for (uint16_t id = 0; id < m_Protos.size(); ++id) 
        {
            ZN_CORE_ASSERT(m_Protos[id].CodeCount > 0, 
                        "Unfilled proto at freeze — reserved but never filled");
        }
        m_Code.shrink_to_fit();
        m_Constants.shrink_to_fit();
        m_Frozen = true;
    }

    void Module::Clear()
    {
        m_Protos.clear();
        m_Constants.clear();
        m_Code.clear();
        m_EntryProto = INVALID_PROTO;
        m_HasEntry = false;
        m_Frozen = false;
    }
}
