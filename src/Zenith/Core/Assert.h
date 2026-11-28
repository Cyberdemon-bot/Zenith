#pragma once

#include "Zenith/Core/Base.h"

#include <string_view>

#ifdef ZN_ENABLE_ASSERTS
    #include <cstddef>
    #include <cstdlib>
    #include <format>
    #include <utility>
#endif

namespace Zenith {

    struct AssertInfo
    {
        bool is_core = true;
        const char* expr = "";
        const char* file = "";
        int line = 0;
        const char* func = "";
        std::string_view message;
    };

    using AssertLogger  = void(*)(const AssertInfo& info);
    using AssertBreaker = void(*)(const AssertInfo& info);

    void SetAssertLogger(AssertLogger logger) noexcept;
    void SetAssertBreaker(AssertBreaker breaker) noexcept;

#ifdef ZN_ENABLE_ASSERTS
    namespace Detail {

        // Returns true if the caller macro should execute default break (ZN_DEBUGBREAK + std::abort).
        bool DispatchAssert(bool is_core,
                            const char* expr,
                            const char* file,
                            int line,
                            const char* func,
                            std::string_view message) noexcept;

        inline bool ReportAssert(bool is_core,
                                 const char* expr,
                                 const char* file,
                                 int line,
                                 const char* func,
                                 std::string_view message) noexcept
        {
            return DispatchAssert(is_core, expr, file, line, func, message);
        }

        template<typename... Args>
        inline bool ReportAssert(bool is_core,
                                 const char* expr,
                                 const char* file,
                                 int line,
                                 const char* func,
                                 std::format_string<Args...> fmt,
                                 Args&&... args) noexcept
        {
            if constexpr (sizeof...(Args) == 0)
            {
                return DispatchAssert(is_core, expr, file, line, func, fmt.get());
            }
            else
            {
                char buf[512];
                auto res = std::format_to_n(buf, sizeof(buf) - 1, fmt, std::forward<Args>(args)...);
                *res.out = '\0';
                return DispatchAssert(is_core, expr, file, line, func,
                                      std::string_view(buf, static_cast<size_t>(res.out - buf)));
            }
        }

    }
#endif

}

#ifdef ZN_ENABLE_ASSERTS

    #define ZN_CORE_ASSERT(check, ...)                                                      \
        do {                                                                                \
            if (!(check)) [[unlikely]]                                                      \
            {                                                                               \
                if (::Zenith::Detail::ReportAssert(true, #check, __FILE__, __LINE__,        \
                                                   __func__, __VA_ARGS__))                  \
                {                                                                           \
                    ZN_DEBUGBREAK();                                                        \
                    std::abort();                                                           \
                }                                                                           \
            }                                                                               \
        } while (0)

    #define ZN_ASSERT(check, ...)                                                           \
        do {                                                                                \
            if (!(check)) [[unlikely]]                                                      \
            {                                                                               \
                if (::Zenith::Detail::ReportAssert(false, #check, __FILE__, __LINE__,       \
                                                   __func__, __VA_ARGS__))                  \
                {                                                                           \
                    ZN_DEBUGBREAK();                                                        \
                    std::abort();                                                           \
                }                                                                           \
            }                                                                               \
        } while (0)

#else
    #define ZN_CORE_ASSERT(...) ((void)0)
    #define ZN_ASSERT(...)      ((void)0)
#endif
