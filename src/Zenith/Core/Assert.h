#pragma once

#include "Zenith/Core/Base.h"

#ifdef ZN_ENABLE_ASSERTS

    // Core assert - message is REQUIRED
    #define ZN_CORE_ASSERT(check, msg, ...) \
        { if(!(check)) { \
            ZN_DEBUGBREAK(); \
        } }
    
    // Client assert - message is REQUIRED
    #define ZN_ASSERT(check, msg, ...) \
        { if(!(check)) { \
            ZN_DEBUGBREAK(); \
        } }

#else
    #define ZN_CORE_ASSERT(...)
    #define ZN_ASSERT(...)
#endif