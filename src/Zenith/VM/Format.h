#pragma once

#include <cstdint>

namespace Zenith {

    enum class Format : uint8_t
    {
        iNone,   // op(8)
        iABC,    // op(8) a(8) b(8) c(8)
        iABx,    // op(8) a(8) Bx(16)
        iAsBx,   // op(8) a(8) sBx(16)
        iAx,     // op(8) Ax(24)
    };

}