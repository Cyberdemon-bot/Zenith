#pragma once

#include "Zenith/VM/Proto.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace Zenith {

    struct CallFrame
    {
        Value* stack_base;
        const Instruction* ip;
        uint16_t proto_id;
    };

    class State
    {
    public:
        State();

        void set_module(std::shared_ptr<const Module> module);
        void prepare(uint16_t entry_proto_id = 0);

        Value& reg(uint8_t idx) noexcept;
        const Value& reg(uint8_t idx) const noexcept;

        const Module& module() const noexcept { return *m_Module; }
    private:
        std::vector<Value> m_Stack;
        std::vector<CallFrame> m_Frames;

        std::shared_ptr<const Module> m_Module;

        CallFrame& current_frame() noexcept { return m_Frames.back(); }
        const CallFrame& current_frame() const noexcept { return m_Frames.back(); }

        void push_frame(Value* stack_base, uint16_t proto_id, const Instruction* ip);
        void pop_frame() noexcept { m_Frames.pop_back(); }

        size_t frame_depth() const noexcept { return m_Frames.size(); }
        bool frames_empty() const noexcept { return m_Frames.empty(); }

        friend class Executor;
    };
}