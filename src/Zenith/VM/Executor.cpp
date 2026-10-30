#include "Zenith/VM/Executor.h"

#define ZN_EXTRACT(fmt, ...)                                            \
    static_assert(fm == Format::fmt,                                    \
                  "opcode format mismatch: " #fmt " expected");         \
    auto [__VA_ARGS__] = inst.extract<Format::fmt>()

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
            regs[a].get_as<ty>() = regs[b].get_as<ty>() + regs[c].get_as<ty>();
        }
        else if constexpr (op == Opcode::Sub) 
        {
            ZN_EXTRACT(iABC, a, b, c);
            regs[a].get_as<ty>() = regs[b].get_as<ty>() - regs[c].get_as<ty>();
        }
        else if constexpr (op == Opcode::Mul) 
        {
            ZN_EXTRACT(iABC, a, b, c);
            regs[a].get_as<ty>() = regs[b].get_as<ty>() * regs[c].get_as<ty>();
        }
        else if constexpr (op == Opcode::Div || op == Opcode::DivS || op == Opcode::DivU) 
        {
            ZN_EXTRACT(iABC, a, b, c);
            regs[a].get_as<ty>() = regs[b].get_as<ty>() / regs[c].get_as<ty>();
        }
        else if constexpr (op == Opcode::RemS || op == Opcode::RemU) 
        {
            ZN_EXTRACT(iABC, a, b, c);
            regs[a].get_as<ty>() = regs[b].get_as<ty>() % regs[c].get_as<ty>();
        }
        else if constexpr (op == Opcode::Neg) 
        {
            ZN_EXTRACT(iABC, a, b, c);
            regs[a].get_as<ty>() = -regs[b].get_as<ty>();
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

        }
        else if constexpr (op == Opcode::Return)
        {

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
        const Instruction* ip = frame.ip;
        Value* regs = frame.stack_base;
        const Value* consts = frame.proto->get_constants().data();

    #if ZN_HAS_MUSTTAIL
        STEP_TABLE[ip->slot()](&state, ip, regs, consts);
    #else
        while ((ip = STEP_TABLE[ip->slot()](&state, ip, regs, consts)) != nullptr) {}
    #endif
    }
}