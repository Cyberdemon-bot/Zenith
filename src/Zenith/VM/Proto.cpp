#include "Zenith/VM/Proto.h"

#include "Zenith/Core/Assert.h"
#include "Zenith/Core/Base.h"

#include <utility>

namespace Zenith {

    uint16_t ProtoBuilder::push_constant(Value value) 
    {
        ZN_CORE_ASSERT(m_Constants.size() <= MAX_Bx, "Constant pool overflow (max 65536)");
        uint16_t idx = static_cast<uint16_t>(m_Constants.size());
        m_Constants.push_back(value);
        return idx;
    }

    uint32_t ProtoBuilder::commit_instruction(Instruction instruction)
    {
        uint32_t idx = static_cast<uint32_t>(m_Code.size());
        m_Code.push_back(instruction);
        return idx;
    }

    void ProtoBuilder::patch_jmp(uint32_t idx, int16_t address)
    {
        ZN_CORE_ASSERT(idx < m_Code.size(), "Instruction index out of bounds");
        m_Code[idx].patch_sBx(address);
    }

    const Proto& Module::proto(uint16_t id) const 
    {
        ZN_CORE_ASSERT(id < m_Protos.size(), "Invalid proto id");
        return m_Protos[id];
    }

    std::span<const Instruction> Module::code(uint16_t id) const 
    {
        const Proto& p = m_Protos[id];
        return { m_Code.data() + p.code_offset, p.code_count };
    }

    std::span<const Value> Module::consts(uint16_t id) const 
    {
        const Proto& p = m_Protos[id];
        return { m_Constants.data() + p.const_offset, p.const_count };
    }

    uint16_t Module::reserve() 
    {
        ZN_CORE_ASSERT(!m_Frozen, "Module is frozen");
        uint16_t id = static_cast<uint16_t>(m_Protos.size());
        m_Protos.emplace_back();
        return id;
    }

    uint16_t Module::add(ProtoBuilder&& b) 
    {
        uint16_t id = reserve();
        fill(id, std::move(b));
        return id;
    }

    void Module::fill(uint16_t id, ProtoBuilder&& b) 
    {
        ZN_CORE_ASSERT(!m_Frozen, "Module is frozen");
        ZN_CORE_ASSERT(id < m_Protos.size(), "Invalid reserved id");
        Proto& p = m_Protos[id];
        ZN_CORE_ASSERT(p.code_count == 0, "Proto already filled");

        p.code_offset = static_cast<uint32_t>(m_Code.size());
        p.code_count = static_cast<uint32_t>(b.m_Code.size());
        p.const_offset = static_cast<uint32_t>(m_Constants.size());
        p.const_count = static_cast<uint32_t>(b.m_Constants.size());
        p.first_child = b.m_FirstChild;
        p.child_count = b.m_ChildCount;
        p.num_param = b.m_NumParams;
        p.max_regs = b.m_MaxRegs;

        m_Code.insert(m_Code.end(), b.m_Code.begin(), b.m_Code.end());
        m_Constants.insert(m_Constants.end(), b.m_Constants.begin(), b.m_Constants.end());
    }

    void Module::freeze() 
    {
        ZN_CORE_ASSERT(!m_Frozen, "Already frozen");
        for (uint16_t id = 0; id < m_Protos.size(); ++id) 
        {
            ZN_CORE_ASSERT(m_Protos[id].code_count > 0, 
                        "Unfilled proto at freeze — reserved but never filled");
        }
        m_Code.shrink_to_fit();
        m_Constants.shrink_to_fit();
        m_Frozen = true;
    }
}