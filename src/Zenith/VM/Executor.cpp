#include "Zenith/VM/Executor.h"

namespace Zenith {

    template<uint32_t Slot>
    StepRet Executor::step(State* state, const Instruction* ip, Value* regs, const Value* consts)
    {
        constexpr SlotInfo info = SLOT_INFO[Slot];
        constexpr Opcode op = info.op;
        constexpr Type ty = info.ty;
        const Instruction inst = *ip++;

        if constexpr (op == Opcode::LoadConst) 
        {
            auto [a, bx] = inst.extract<Format::iABx>();
            regs[a].get_as<ty>() = consts[bx].get_as<ty>();
        }
        else if constexpr (op == Opcode::Add) 
        {
            auto [a, b, c] = inst.extract<Format::iABC>();
            regs[a].get_as<ty>() = regs[b].get_as<ty>() + regs[c].get_as<ty>();
        }
        else if constexpr (op == Opcode::Sub) 
        {
            auto [a, b, c] = inst.extract<Format::iABC>();
            regs[a].get_as<ty>() = regs[b].get_as<ty>() - regs[c].get_as<ty>();
        }
        else if constexpr (op == Opcode::Mul) 
        {
            auto [a, b, c] = inst.extract<Format::iABC>();
            regs[a].get_as<ty>() = regs[b].get_as<ty>() * regs[c].get_as<ty>();
        }
        else if constexpr (op == Opcode::Div || op == Opcode::DivS || op == Opcode::DivU) 
        {
            auto [a, b, c] = inst.extract<Format::iABC>();
            regs[a].get_as<ty>() = regs[b].get_as<ty>() / regs[c].get_as<ty>();
        }
        else if constexpr (op == Opcode::RemS || op == Opcode::RemU) 
        {
            auto [a, b, c] = inst.extract<Format::iABC>();
            regs[a].get_as<ty>() = regs[b].get_as<ty>() % regs[c].get_as<ty>();
        }
        else if constexpr (op == Opcode::Neg) 
        {
            auto [a, b, c] = inst.extract<Format::iABC>();
            regs[a].get_as<ty>() = -regs[b].get_as<ty>();
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