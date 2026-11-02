#include "Zenith/VM/State.h"

#include "Zenith/Core/Assert.h"
#include "Zenith/Core/Base.h"

#include <utility>

namespace Zenith {

    State::State()
    {
        m_Stack.resize(DEFAULT_STACK_SIZE); 
        m_Frames.reserve(MAX_CALL_FRAMES);
    }

    void State::set_module(std::shared_ptr<const Module> module)
    {
        ZN_CORE_ASSERT(module != nullptr, "Cannot set null module");
        ZN_CORE_ASSERT(module->is_frozen(), "Module must be frozen");
        ZN_CORE_ASSERT(m_Frames.empty(), "Cannot swap module while frames active");
        m_Module = std::move(module);
    }

    void State::prepare(uint16_t entry_proto_id)
    {
        ZN_CORE_ASSERT(m_Module != nullptr, "Module not set");
        ZN_CORE_ASSERT(entry_proto_id < m_Module->num_protos(), "Invalid entry proto");
        m_Frames.clear();

        m_Frames.push_back(CallFrame
        {
            .stack_base = m_Stack.data(),
            .ip = m_Module->code(entry_proto_id).data(),
            .proto_id = entry_proto_id,
        });
    }

    Value& State::reg(uint8_t idx) noexcept
    {
        ZN_CORE_ASSERT(!m_Frames.empty(), "reg() called after all frames popped");
        return m_Frames.back().stack_base[idx];
    }

    const Value& State::reg(uint8_t idx) const noexcept
    {
        ZN_CORE_ASSERT(!m_Frames.empty(), "reg() called after all frames popped");
        return m_Frames.back().stack_base[idx];
    }
    
    void State::push_frame(Value* stack_base, uint16_t proto_id, const Instruction* ip)
    {
        ZN_CORE_ASSERT(m_Frames.size() < MAX_CALL_FRAMES, "Call stack overflow");
        ZN_CORE_ASSERT((stack_base - m_Stack.data()) + m_Module->proto(proto_id).max_regs()
                        <= m_Stack.size(), "Register stack overflow");
        m_Frames.push_back(CallFrame{ stack_base, ip, proto_id });
    }
}