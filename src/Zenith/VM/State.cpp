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

    void State::SetModule(Ref<const Module> module)
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
        m_Error = {};

        m_Frames.push_back(CallFrame
        {
            .StackBase = m_Stack.data(),
            .IP = m_Module->GetCode(entryProtoId).data(),
            .ProtoId = entryProtoId,
        });
    }

    void State::Reset()
    {
        m_Frames.clear();
        m_Error = {};
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

    RuntimeErrorCode State::PushFrame(Value* stackBase, uint16_t protoId, const Instruction* ip) noexcept
    {
        if (m_Frames.size() >= MAX_CALL_FRAMES) [[unlikely]]
            return RuntimeErrorCode::CallStackOverflow;
        if (stackBase < m_Stack.data() ||
            static_cast<size_t>(stackBase - m_Stack.data()) + m_Module->GetProto(protoId).MaxRegs > m_Stack.size()) [[unlikely]]
            return RuntimeErrorCode::RegisterStackOverflow;

        m_Frames.push_back(CallFrame{ stackBase, ip, protoId });
        return RuntimeErrorCode::None;
    }

    void State::Trap(RuntimeErrorCode code, const Instruction* fault_ip) noexcept
    {
        ZN_CORE_ASSERT(!m_Frames.empty(), "Trap() called without active frame");
        CallFrame& cur = m_Frames.back();
        auto code_span = m_Module->GetCode(cur.ProtoId);
        uint32_t pc = 0;
        if (fault_ip >= code_span.data() && fault_ip < code_span.data() + code_span.size())
            pc = static_cast<uint32_t>(fault_ip - code_span.data());

        cur.IP = fault_ip;
        m_Error = RuntimeError{ .code = code, .proto_id = cur.ProtoId, .pc = pc };
    }
}
