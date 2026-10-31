#pragma once

#include "Zenith/Core/Base.h"

#ifdef ZN_ENABLE_ASSERTS

    // Core assert - message is REQUIRED
    #define ZN_CORE_ASSERT(check, msg, ...) \
        do { if(!(check)) { \
            ZN_DEBUGBREAK(); \
        } } while(0)
    
    // Client assert - message is REQUIRED
    #define ZN_ASSERT(check, msg, ...) \
        do { if(!(check)) { \
            ZN_DEBUGBREAK(); \
        } } while(0)

#else
    #define ZN_CORE_ASSERT(...)
    #define ZN_ASSERT(...)
#endif