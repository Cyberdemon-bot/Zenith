#pragma once

#include "Zenith/VM/Proto.h"
#include "Zenith/VM/Instruction.h"

#include <span>

namespace Zenith {

    struct CallFrame
    {
        Value* stack_base;
        const Proto* proto;
        const Instruction* ip;
    };

    class State
    {
    public:
        State();

        uint16_t add_proto(Proto&& proto);
        void prepare(uint16_t entry_proto_id = 0);

        Value& reg(uint8_t idx) noexcept { return m_Frames.back().stack_base[idx]; }
        const Value& reg(uint8_t idx) const noexcept { return m_Frames.back().stack_base[idx]; }
    private:
        std::vector<Value> m_Stack;
        std::vector<Proto> m_Protos;
        std::vector<CallFrame> m_Frames;

        CallFrame& current_frame() noexcept { return m_Frames.back(); }
        const Value& get_stack_value(size_t idx) const noexcept { return m_Stack[idx]; }

        friend class Executor;
    };
}