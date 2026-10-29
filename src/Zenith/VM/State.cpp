#include "Zenith/Core/Base.h"
#include "Zenith/VM/State.h"

namespace Zenith {

    State::State()
    {
        m_Protos.reserve(64);
        m_Stack.resize(DEFAULT_STACK_SIZE); 
        m_Frames.reserve(MAX_CALL_FRAMES);
    }

    uint16_t State::add_proto(Proto&& proto)
    {
        ZN_CORE_ASSERT(m_Frames.empty(), "Cannot add new Proto while VM frames are active (prevents pointer invalidation)");
        ZN_CORE_ASSERT(m_Protos.size() <= MAX_Bx, "Proto pool overflow");
        
        uint16_t id = static_cast<uint16_t>(m_Protos.size());
        m_Protos.push_back(std::move(proto));
        return id;
    }

    void State::prepare(uint16_t entry_proto_id)
    {
        ZN_CORE_ASSERT(entry_proto_id < m_Protos.size(), "Invalid entry proto ID");
        m_Frames.clear();

        const Proto* target = &m_Protos[entry_proto_id];
        m_Frames.push_back(CallFrame
        {
            .stack_base = m_Stack.data(),
            .proto      = target,
            .ip         = target->get_code().data(),
        });
    }
}