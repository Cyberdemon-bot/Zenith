#include "Zenith/VM/Executor.h"

#include "Zenith/Core/Base.h"
#include "Zenith/VM/Instruction.h"
#include "Zenith/VM/State.h"

#include <cstddef>
#include <cstdint>
#include <utility>

#define ZN_EXTRACT(fmt, ...)                                            \
    static_assert(fm == Format::fmt,                                    \
                  "opcode format mismatch: " #fmt " expected");         \
    auto [__VA_ARGS__] = inst.Extract<Format::fmt>();

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
        else if constexpr (op == Opcode::Div || op == Opcode::DivS || op == Opcode::DivU) 
        {
            ZN_EXTRACT(iABC, a, b, c);
            regs[a].SetAs<ty>(regs[b].GetAs<ty>() / regs[c].GetAs<ty>());
        }
        else if constexpr (op == Opcode::RemS || op == Opcode::RemU) 
        {
            ZN_EXTRACT(iABC, a, b, c);
            regs[a].SetAs<ty>(regs[b].GetAs<ty>() % regs[c].GetAs<ty>());
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
        }
        else if constexpr (op == Opcode::Jz) 
        {
            ZN_EXTRACT(iAsBx, a, sbx);
            if (regs[a].u64 == 0) ip += sbx;
        }
        else if constexpr (op == Opcode::Jnz) 
        {
            ZN_EXTRACT(iAsBx, a, sbx);
            if (regs[a].u64 != 0) ip += sbx;
        }
        else if constexpr (op == Opcode::Call)
        {
            ZN_EXTRACT(iABx, a, Bx);
            state->CurrentFrame().IP = ip;
            const Module& module = state->GetModule();

            regs += a;
            ip = module.GetCode(Bx).data();        
            consts = module.GetConsts(Bx).data();    
            state->PushFrame(regs, Bx, ip);
        }
        else if constexpr (op == Opcode::Return)
        {
            ZN_EXTRACT(iABC, a, b, c);
            if (b != 0 && a != 0) // copy return values back to regs[0] of callee
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

    void Executor::Execute(State& state) noexcept 
    {
        CallFrame& frame = state.CurrentFrame();
        const Module& module = state.GetModule();

        const Instruction* ip = frame.IP;
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
    }
}
