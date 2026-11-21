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

    void State::SetModule(std::shared_ptr<const Module> module)
    {
        ZN_CORE_ASSERT(module != nullptr, "Cannot set null module");
        ZN_CORE_ASSERT(module->IsFrozen(), "Module must be frozen");
        ZN_CORE_ASSERT(m_Frames.empty(), "Cannot swap module while frames active");
        m_Module = std::move(module);
    }

    void State::Prepare(uint16_t entryProtoId)
    {
        ZN_CORE_ASSERT(m_Module != nullptr, "Module not set");
        ZN_CORE_ASSERT(entryProtoId < m_Module->GetNumProtos(), "Invalid entry proto");
        m_Frames.clear();

        m_Frames.push_back(CallFrame
        {
            .StackBase = m_Stack.data(),
            .IP = m_Module->GetCode(entryProtoId).data(),
            .ProtoId = entryProtoId,
        });
    }

    Value& State::Reg(uint8_t idx) noexcept
    {
        ZN_CORE_ASSERT(!m_Frames.empty(), "Reg() called after all frames popped");
        return m_Frames.back().StackBase[idx];
    }

    const Value& State::Reg(uint8_t idx) const noexcept
    {
        ZN_CORE_ASSERT(!m_Frames.empty(), "Reg() called after all frames popped");
        return m_Frames.back().StackBase[idx];
    }
    
    void State::PushFrame(Value* stackBase, uint16_t protoId, const Instruction* ip)
    {
        ZN_CORE_ASSERT(m_Frames.size() < MAX_CALL_FRAMES, "Call stack overflow");
        ZN_CORE_ASSERT(stackBase >= m_Stack.data() &&
                       static_cast<size_t>(stackBase - m_Stack.data()) + m_Module->GetProto(protoId).MaxRegs
                        <= m_Stack.size(), "Register stack overflow");
        m_Frames.push_back(CallFrame{ stackBase, ip, protoId });
    }
}
