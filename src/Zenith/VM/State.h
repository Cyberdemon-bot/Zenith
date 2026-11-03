#pragma once

#include "Zenith/VM/Proto.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace Zenith {

    struct CallFrame
    {
        Value* StackBase;
        const Instruction* IP;
        uint16_t ProtoId;
    };

    class State
    {
    public:
        State();

        void SetModule(std::shared_ptr<const Module> module);
        void Prepare(uint16_t entryProtoId = 0);

        Value& Reg(uint8_t idx) noexcept;
        const Value& Reg(uint8_t idx) const noexcept;

        const Module& GetModule() const noexcept { return *m_Module; }
    private:
        std::vector<Value> m_Stack;
        std::vector<CallFrame> m_Frames;

        std::shared_ptr<const Module> m_Module;

        CallFrame& CurrentFrame() noexcept { return m_Frames.back(); }
        const CallFrame& CurrentFrame() const noexcept { return m_Frames.back(); }

        void PushFrame(Value* stackBase, uint16_t protoId, const Instruction* ip);
        void PopFrame() noexcept { m_Frames.pop_back(); }

        size_t GetFrameDepth() const noexcept { return m_Frames.size(); }
        bool IsFramesEmpty() const noexcept { return m_Frames.empty(); }

        friend class Executor;
    };
}
