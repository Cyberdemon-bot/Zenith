#pragma once

#include "Zenith/Core/Base.h"
#include "Zenith/VM/Proto.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace Zenith {

    enum class RuntimeErrorCode : uint8_t
    {
        None = 0,
        DivisionByZero,
        IntegerOverflow,
        CallStackOverflow,
        RegisterStackOverflow,
    };

    constexpr std::string_view RuntimeErrorToString(RuntimeErrorCode code) noexcept
    {
        switch (code)
        {
            case RuntimeErrorCode::None:                  return "None";
            case RuntimeErrorCode::DivisionByZero:        return "Division by zero";
            case RuntimeErrorCode::IntegerOverflow:       return "Signed integer division overflow";
            case RuntimeErrorCode::CallStackOverflow:     return "Call stack overflow";
            case RuntimeErrorCode::RegisterStackOverflow: return "Register stack overflow";
            default:                                      return "Unknown runtime error";
        }
    }

    struct RuntimeError
    {
        RuntimeErrorCode code = RuntimeErrorCode::None;
        uint16_t proto_id = INVALID_PROTO;
        uint32_t pc = 0;

        constexpr bool Ok() const noexcept { return code == RuntimeErrorCode::None; }
    };

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
        bool HasError() const noexcept { return !m_Error.Ok(); }
        const RuntimeError& GetError() const noexcept { return m_Error; }
        std::span<const CallFrame> Frames() const noexcept { return m_Frames; }
    private:
        std::vector<Value> m_Stack;
        std::vector<CallFrame> m_Frames;
        RuntimeError m_Error;

        std::shared_ptr<const Module> m_Module;

        CallFrame& CurrentFrame() noexcept { return m_Frames.back(); }
        const CallFrame& CurrentFrame() const noexcept { return m_Frames.back(); }

        RuntimeErrorCode PushFrame(Value* stackBase, uint16_t protoId, const Instruction* ip) noexcept;
        void PopFrame() noexcept { m_Frames.pop_back(); }
        void Trap(RuntimeErrorCode code, const Instruction* fault_ip) noexcept;

        size_t GetFrameDepth() const noexcept { return m_Frames.size(); }
        bool IsFramesEmpty() const noexcept { return m_Frames.empty(); }

        friend class Executor;
    };
}
