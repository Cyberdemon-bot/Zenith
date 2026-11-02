#pragma once

#include "Zenith/VM/Instruction.h"
#include "Zenith/VM/Value.h"

#include <cstdint>
#include <span>
#include <vector>

namespace Zenith {

    class Module;
    struct Proto 
    {
        Proto() = default;

        uint32_t code_offset  = 0;
        uint32_t code_count   = 0;
        uint32_t const_offset = 0;
        uint32_t const_count  = 0;
        uint16_t first_child  = 0;
        uint16_t child_count  = 0;
        uint8_t  num_param    = 0;
        uint8_t  max_regs     = 0;
        uint8_t  num_return   = 0;
    };

    class ProtoBuilder
    {
    public:
        ProtoBuilder() = default;

        uint16_t push_constant(Value value);
        uint32_t commit_instruction(Instruction instruction);

        void patch_jmp(uint32_t idx, int16_t sbx);
        void set_num_params(uint8_t n) { m_NumParams = n; }
        void set_num_return(uint8_t n) { m_NumReturn = n; }
        void set_max_regs(uint8_t n) { m_MaxRegs = n; }

        uint8_t num_params() const { return m_NumParams; }
        uint8_t num_return() const { return m_NumReturn; }
        uint8_t max_regs() const { return m_MaxRegs; }
        uint32_t code_size() const  { return static_cast<uint32_t>(m_Code.size()); }
        uint16_t const_size() const { return static_cast<uint16_t>(m_Constants.size()); }

        void set_children(uint16_t first, uint16_t count) 
        {
            m_FirstChild = first;
            m_ChildCount = count;
        }

        void clear() 
        {
            m_Code.clear(); m_Constants.clear();
            m_FirstChild = m_ChildCount = 0;
            m_NumParams = m_MaxRegs = 0;
        }
    private:
        std::vector<Instruction> m_Code;
        std::vector<Value> m_Constants;
        uint16_t m_FirstChild = 0, m_ChildCount = 0;
        uint8_t m_NumParams = 0, m_NumReturn = 0, m_MaxRegs = 0;

        friend class Module;
    };

    class Module
    {
    public:
        Module() = default;
        Module(Module&&) = default;
        Module& operator=(Module&&) = default;
        Module(const Module&) = delete;            
        Module& operator=(const Module&) = delete;

        const Proto& proto(uint16_t id) const;
        uint16_t num_protos() const { return static_cast<uint16_t>(m_Protos.size()); }
        std::span<const Instruction> code(uint16_t id) const;
        std::span<const Value> consts(uint16_t id) const;

        uint16_t reserve();
        uint16_t add(ProtoBuilder&& b);
        void fill(uint16_t id, ProtoBuilder&& b);

        void freeze();
        bool is_frozen() const { return m_Frozen; }
    private:
        std::vector<Proto> m_Protos;
        std::vector<Value> m_Constants;
        std::vector<Instruction> m_Code;
        bool m_Frozen = false;
    };
}