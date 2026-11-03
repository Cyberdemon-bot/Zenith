#pragma once

#include <cstdint>

namespace Zenith {

    enum class CTType : uint8_t // compile-time
    {
        Error = 0,
        Unit,      
        Bool,
        I64,
        F64,
    };
}