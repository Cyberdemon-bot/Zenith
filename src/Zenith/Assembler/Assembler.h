#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Zenith/Core/Base.h"

namespace Zenith {

    struct AssembledProgram
    {
        std::vector<Instruction> code;
        std::vector<uint64_t> constants;
        uint32_t register_count = 0;
    };

    class Assembler
    {
    public:
        AssembledProgram assemble(const std::string& source) const;
    };

} 
