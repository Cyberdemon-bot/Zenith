#include "Zenith/Core/Assert.h"
#include "Zenith/VM/Proto.h"

namespace Zenith {

    Proto::Proto(uint8_t num_params, uint8_t max_regs)
        : m_NumParams(num_params), m_MaxRegisters(max_regs)
    {
        m_Constants.reserve(32);
        m_Instructions.reserve(32);
    }
        
    uint16_t Proto::push_constant(Value value) 
    {
        ZN_CORE_ASSERT(m_Constants.size() <= MAX_Bx, "Constant pool overflow (max 65536)");
        uint16_t idx = static_cast<uint16_t>(m_Constants.size());
        m_Constants.push_back(value);
        return idx;
    }

    uint32_t Proto::commit_instruction(Instruction instruction)
    {
        uint32_t idx = static_cast<uint32_t>(m_Instructions.size());
        m_Instructions.push_back(instruction);
        return idx;
    }

    void Proto::patch_jmp(uint32_t idx, int16_t address)
    {
        ZN_CORE_ASSERT(idx < m_Instructions.size(), "Instruction index out of bounds");
        m_Instructions[idx].patch_sBx(address);
    }

    void Proto::set_max_regs(uint8_t max_regs)
    {
        m_MaxRegisters = max_regs;
    }
}