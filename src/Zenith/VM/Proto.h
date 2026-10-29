#pragma once

#include "Zenith/VM/Value.h"
#include "Zenith/VM/Instruction.h"

#include <vector>
#include <utility>
#include <span>

namespace Zenith {

    class Proto
    {
    public:
        Proto(uint8_t num_params = 0, uint8_t max_regs = 0);

        uint16_t push_constant(Value value);
        uint32_t commit_instruction(Instruction instruction);
        void patch_jmp(uint32_t idx, int16_t address);
        void set_max_regs(uint8_t max_regs);

        std::span<const Value> get_constants() const { return m_Constants; }
        std::span<const Instruction> get_code() const { return m_Instructions; }
        std::pair<uint8_t, uint8_t> get_param_and_reg() const { return { m_NumParams, m_MaxRegisters }; }
    private:
        std::vector<Value> m_Constants;
        std::vector<uint32_t> m_LineInfo; // TODO: sync to instruction
        std::vector<Instruction> m_Instructions;
        uint16_t m_FirstChild = 0, m_ChildCount = 0;
        uint8_t m_NumParams;
        uint8_t m_MaxRegisters;
    };
    
}