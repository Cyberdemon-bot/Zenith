#pragma once

#include <span>
#include "Zenith/Core/Base.h"

namespace Zenith {

    struct Proto // function prototype
    {
        std::span<Instruction> code;
        std::span<uint64_t> numeric_constants; // unified buffer for any 64bit data
        uint32_t register_count = 0;
    };
}