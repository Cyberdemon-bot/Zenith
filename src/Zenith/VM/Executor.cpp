#include "Zenith/VM/Executor.h"

#include "Zenith/Core/Assert.h"
#include "Zenith/Core/Base.h"
#include "Zenith/VM/Instruction.h"
#include "Zenith/VM/State.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

#define ZN_EXTRACT(fmt, ...)                                            \
    static_assert(fm == Format::fmt,                                    \
                  "opcode format mismatch: " #fmt " expected");         \
    auto [__VA_ARGS__] = inst.Extract<Format::fmt>();

#define ZN_TRAP(err_code)                                               \
    do { state->Trap(err_code, ip - 1); ZN_HALT(); } while (0)

namespace Zenith {

    template<uint32_t Slot>
    StepRet Executor::Step(State* state, const Instruction* ip, Value* regs, const Value* consts)
    {
        constexpr SlotInfo info = SLOT_INFO[Slot];
        constexpr Opcode op = info.op;
        constexpr Type ty = info.ty;
        constexpr Format fm = info.format;
        const Instruction inst = *ip++;

        if constexpr (op == Opcode::LoadConst)
        {
            ZN_EXTRACT(iABx, a, bx);
            ZN_CORE_ASSERT(bx < state->GetModule().GetProto(state->CurrentFrame().ProtoId).ConstCount,
                           "Constant index out of bounds");
            regs[a].u64 = consts[bx].u64;
        }
        else if constexpr (op == Opcode::LoadImm)
        {
            ZN_EXTRACT(iAsBx, a, sbx);
            regs[a].i64 = static_cast<int64_t>(sbx);
        }
        else if constexpr (op == Opcode::Mov)
        {
            ZN_EXTRACT(iABC, a, b, c);
            regs[a].u64 = regs[b].u64;
        }
        else if constexpr (op == Opcode::Add)
        {
            ZN_EXTRACT(iABC, a, b, c);
            regs[a].SetAs<ty>(regs[b].GetAs<ty>() + regs[c].GetAs<ty>());
        }
        else if constexpr (op == Opcode::Sub)
        {
            ZN_EXTRACT(iABC, a, b, c);
            regs[a].SetAs<ty>(regs[b].GetAs<ty>() - regs[c].GetAs<ty>());
        }
        else if constexpr (op == Opcode::Mul)
        {
            ZN_EXTRACT(iABC, a, b, c);
            regs[a].SetAs<ty>(regs[b].GetAs<ty>() * regs[c].GetAs<ty>());
        }
        else if constexpr (op == Opcode::Div)
        {
            ZN_EXTRACT(iABC, a, b, c);
            regs[a].SetAs<ty>(regs[b].GetAs<ty>() / regs[c].GetAs<ty>());
        }
        else if constexpr (op == Opcode::DivS)
        {
            ZN_EXTRACT(iABC, a, b, c);
            auto lhs = regs[b].GetAs<ty>();
            auto rhs = regs[c].GetAs<ty>();
            if (rhs == 0) [[unlikely]] ZN_TRAP(RuntimeErrorCode::DivisionByZero);
            if (lhs == std::numeric_limits<decltype(lhs)>::min() && rhs == -1) [[unlikely]]
                ZN_TRAP(RuntimeErrorCode::IntegerOverflow);
            regs[a].SetAs<ty>(lhs / rhs);
        }
        else if constexpr (op == Opcode::DivU)
        {
            ZN_EXTRACT(iABC, a, b, c);
            auto rhs = regs[c].GetAs<ty>();
            if (rhs == 0) [[unlikely]] ZN_TRAP(RuntimeErrorCode::DivisionByZero);
            regs[a].SetAs<ty>(regs[b].GetAs<ty>() / rhs);
        }
        else if constexpr (op == Opcode::RemS)
        {
            ZN_EXTRACT(iABC, a, b, c);
            auto lhs = regs[b].GetAs<ty>();
            auto rhs = regs[c].GetAs<ty>();
            if (rhs == 0) [[unlikely]] ZN_TRAP(RuntimeErrorCode::DivisionByZero);
            if (lhs == std::numeric_limits<decltype(lhs)>::min() && rhs == -1) [[unlikely]]
                ZN_TRAP(RuntimeErrorCode::IntegerOverflow);
            regs[a].SetAs<ty>(lhs % rhs);
        }
        else if constexpr (op == Opcode::RemU)
        {
            ZN_EXTRACT(iABC, a, b, c);
            auto rhs = regs[c].GetAs<ty>();
            if (rhs == 0) [[unlikely]] ZN_TRAP(RuntimeErrorCode::DivisionByZero);
            regs[a].SetAs<ty>(regs[b].GetAs<ty>() % rhs);
        }
        else if constexpr (op == Opcode::Neg)
        {
            ZN_EXTRACT(iABC, a, b, c);
            regs[a].SetAs<ty>(-regs[b].GetAs<ty>());
        }
        else if constexpr (op == Opcode::Eq)
        {
            ZN_EXTRACT(iABC, a, b, c);
            regs[a].u64 = static_cast<uint64_t>(regs[b].GetAs<ty>() == regs[c].GetAs<ty>());
        }
        else if constexpr (op == Opcode::Lt)
        {
            ZN_EXTRACT(iABC, a, b, c);
            regs[a].u64 = static_cast<uint64_t>(regs[b].GetAs<ty>() < regs[c].GetAs<ty>());
        }
        else if constexpr (op == Opcode::LNot)
        {
            ZN_EXTRACT(iABC, a, b, c);
            regs[a].u64 = (regs[b].u64 == 0) ? 1 : 0;
        }
        else if constexpr (op == Opcode::Jmp)
        {
            ZN_EXTRACT(iAsBx, a, sbx);
            ip += sbx;
            ZN_CORE_ASSERT(ip >= state->GetModule().GetCode(state->CurrentFrame().ProtoId).data() &&
                           ip <  state->GetModule().GetCode(state->CurrentFrame().ProtoId).data() +
                                 state->GetModule().GetCode(state->CurrentFrame().ProtoId).size(),
                           "Jump target out of bounds");
        }
        else if constexpr (op == Opcode::Jz)
        {
            ZN_EXTRACT(iAsBx, a, sbx);
            if (regs[a].u64 == 0)
            {
                ip += sbx;
                ZN_CORE_ASSERT(ip >= state->GetModule().GetCode(state->CurrentFrame().ProtoId).data() &&
                               ip <  state->GetModule().GetCode(state->CurrentFrame().ProtoId).data() +
                                     state->GetModule().GetCode(state->CurrentFrame().ProtoId).size(),
                               "Jump target out of bounds");
            }
        }
        else if constexpr (op == Opcode::Jnz)
        {
            ZN_EXTRACT(iAsBx, a, sbx);
            if (regs[a].u64 != 0)
            {
                ip += sbx;
                ZN_CORE_ASSERT(ip >= state->GetModule().GetCode(state->CurrentFrame().ProtoId).data() &&
                               ip <  state->GetModule().GetCode(state->CurrentFrame().ProtoId).data() +
                                     state->GetModule().GetCode(state->CurrentFrame().ProtoId).size(),
                               "Jump target out of bounds");
            }
        }
        else if constexpr (op == Opcode::Call)
        {
            ZN_EXTRACT(iABx, a, Bx);
            const Instruction* call_ip = ip - 1;
            state->CurrentFrame().IP = ip;
            const Module& module = state->GetModule();
            ZN_CORE_ASSERT(Bx < module.GetNumProtos(), "Proto index out of bounds");
            ZN_CORE_ASSERT(module.GetProto(Bx).CodeCount > 0, "Target proto has empty code");

            // Register window overlap: callee R0 aliases caller R[a].
            Value* new_regs = regs + a;
            const Instruction* new_ip = module.GetCode(Bx).data();
            if (auto err = state->PushFrame(new_regs, Bx, new_ip); err != RuntimeErrorCode::None) [[unlikely]]
            {
                state->Trap(err, call_ip);
                ZN_HALT();
            }
            regs = new_regs;
            ip = new_ip;
            consts = module.GetConsts(Bx).data();
        }
        else if constexpr (op == Opcode::Return)
        {
            ZN_EXTRACT(iABC, a, b, c);
            ZN_CORE_ASSERT(static_cast<uint16_t>(a) + b <= 256, "Return register range out of bounds");
            // Copy return values to callee R[0..b-1], which aliases caller R[call_a..call_a+b-1].
            if (b != 0 && a != 0)
                for (uint8_t i = 0; i < b; ++i)
                    regs[i] = regs[a + i];
            if (state->GetFrameDepth() == 1)
            {
                state->CurrentFrame().IP = ip;
                ZN_HALT();
            }

            state->CurrentFrame().IP = ip;
            state->PopFrame();

            const CallFrame& caller = state->CurrentFrame();
            ip = caller.IP;
            regs = caller.StackBase;
            consts = state->GetModule().GetConsts(caller.ProtoId).data();
        }
        else if constexpr (op == Opcode::Halt)
        {
            state->CurrentFrame().IP = ip;
            ZN_HALT();
        }
        else static_assert(always_false_v<Slot>, "unhandled opcode in step");
        ZN_NEXT();
    }

    const std::array<Executor::StepFn, DISPATCH_SIZE> Executor::STEP_TABLE =
        []<std::size_t... Is>(std::index_sequence<Is...>) {
            return std::array<Executor::StepFn, DISPATCH_SIZE>{ &Executor::Step<Is>... };
        }(std::make_index_sequence<DISPATCH_SIZE>{});

    bool Executor::Execute(State& state) noexcept
    {
        ZN_CORE_ASSERT(!state.IsFramesEmpty(), "Execute() called without active frame");
        CallFrame& frame = state.CurrentFrame();
        const Module& module = state.GetModule();
        ZN_CORE_ASSERT(module.GetProto(frame.ProtoId).CodeCount > 0, "Entry proto has empty code");

        const Instruction* ip = frame.IP;
        ZN_CORE_ASSERT(ip != nullptr, "Null instruction pointer");
        Value* regs = frame.StackBase;
        const Value* consts = module.GetConsts(frame.ProtoId).data();

    #if ZN_HAS_MUSTTAIL
        STEP_TABLE[ip->Slot()](&state, ip, regs, consts);
    #else
        while (ip != nullptr)
        {
            const CallFrame& cur = state.CurrentFrame();
            ip = STEP_TABLE[ip->Slot()](&state, ip, cur.StackBase, module.GetConsts(cur.ProtoId).data());
        }
    #endif
        return !state.HasError();
    }
}
