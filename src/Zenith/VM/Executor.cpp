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
    auto [__VA_ARGS__] = inst.extract<Format::fmt>();

namespace Zenith {

    template<uint32_t Slot>
    StepRet Executor::step(State* state, const Instruction* ip, Value* regs, const Value* consts)
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
            regs[a].set_as<ty>(regs[b].get_as<ty>() + regs[c].get_as<ty>());
        }
        else if constexpr (op == Opcode::Sub) 
        {
            ZN_EXTRACT(iABC, a, b, c);
            regs[a].set_as<ty>(regs[b].get_as<ty>() - regs[c].get_as<ty>());
        }
        else if constexpr (op == Opcode::Mul) 
        {
            ZN_EXTRACT(iABC, a, b, c);
            regs[a].set_as<ty>(regs[b].get_as<ty>() * regs[c].get_as<ty>());
        }
        else if constexpr (op == Opcode::Div || op == Opcode::DivS || op == Opcode::DivU) 
        {
            ZN_EXTRACT(iABC, a, b, c);
            regs[a].set_as<ty>(regs[b].get_as<ty>() / regs[c].get_as<ty>());
        }
        else if constexpr (op == Opcode::RemS || op == Opcode::RemU) 
        {
            ZN_EXTRACT(iABC, a, b, c);
            regs[a].set_as<ty>(regs[b].get_as<ty>() % regs[c].get_as<ty>());
        }
        else if constexpr (op == Opcode::Neg) 
        {
            ZN_EXTRACT(iABC, a, b, c);
            regs[a].set_as<ty>(-regs[b].get_as<ty>());
        }
        else if constexpr (op == Opcode::Eq) 
        {
            ZN_EXTRACT(iABC, a, b, c);
            regs[a].u64 = static_cast<uint64_t>(regs[b].get_as<ty>() == regs[c].get_as<ty>());
        }
        else if constexpr (op == Opcode::Lt) 
        {
            ZN_EXTRACT(iABC, a, b, c);
            regs[a].u64 = static_cast<uint64_t>(regs[b].get_as<ty>() < regs[c].get_as<ty>());
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
            state->current_frame().ip = ip;
            const Module& module = state->module();

            regs += a;
            ip = module.code(Bx).data();        
            consts = module.consts(Bx).data();    
            state->push_frame(regs, Bx, ip);
        }
        else if constexpr (op == Opcode::Return)
        {
            ZN_EXTRACT(iABC, a, b, c);
            if (b != 0 && a != 0) // copy return values back to regs[0] of callee
                for (uint8_t i = 0; i < b; ++i)
                    regs[i] = regs[a + i];
            if (state->frame_depth() == 1) 
            {
                state->current_frame().ip = ip;
                ZN_HALT();
            }

            state->current_frame().ip = ip;
            state->pop_frame();
            
            const CallFrame& caller = state->current_frame();
            ip = caller.ip;
            regs = caller.stack_base;
            consts = state->module().consts(caller.proto_id).data();
        }
        else if constexpr (op == Opcode::Halt) 
        {
            state->current_frame().ip = ip;
            ZN_HALT();
        }
        else static_assert(always_false_v<Slot>, "unhandled opcode in step");
        ZN_NEXT();
    }

    const std::array<Executor::StepFn, DISPATCH_SIZE> Executor::STEP_TABLE =
        []<std::size_t... Is>(std::index_sequence<Is...>) {
            return std::array<Executor::StepFn, DISPATCH_SIZE>{ &Executor::step<Is>... };
        }(std::make_index_sequence<DISPATCH_SIZE>{});

    void Executor::execute(State& state) noexcept 
    {
        CallFrame& frame = state.current_frame();
        const Module& module = state.module();

        const Instruction* ip = frame.ip;
        Value* regs = frame.stack_base;
        const Value* consts = module.consts(frame.proto_id).data();

    #if ZN_HAS_MUSTTAIL
        STEP_TABLE[ip->slot()](&state, ip, regs, consts);
    #else
        while (ip != nullptr)
        {
            const CallFrame& cur = state.current_frame();
            ip = STEP_TABLE[ip->slot()](&state, ip, cur.stack_base, module.consts(cur.proto_id).data());
        }
    #endif
    }
}