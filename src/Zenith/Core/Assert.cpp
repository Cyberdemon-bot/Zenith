#include "Zenith/Core/Assert.h"

#include <cstdio>

namespace Zenith {

    namespace {

        void DefaultAssertLogger(const AssertInfo& info)
        {
            std::fprintf(
                stderr,
                "[%s ASSERT FAILED]\n"
                "  Condition : %s\n"
                "  Message   : %.*s\n"
                "  Location  : %s:%d in %s\n",
                info.is_core ? "CORE" : "CLIENT",
                info.expr,
                static_cast<int>(info.message.size()),
                info.message.data(),
                info.file,
                info.line,
                info.func);
            std::fflush(stderr);
        }

        AssertLogger  s_Logger  = &DefaultAssertLogger;
        AssertBreaker s_Breaker = nullptr;

    }

    void SetAssertLogger(AssertLogger logger) noexcept
    {
        s_Logger = logger ? logger : &DefaultAssertLogger;
    }

    void SetAssertBreaker(AssertBreaker breaker) noexcept
    {
        s_Breaker = breaker;
    }

    namespace Detail {

        bool DispatchAssert(bool is_core,
                            const char* expr,
                            const char* file,
                            int line,
                            const char* func,
                            std::string_view message) noexcept
        {
            AssertInfo info
            {
                .is_core = is_core,
                .expr = expr,
                .file = file,
                .line = line,
                .func = func,
                .message = message,
            };

            if (s_Logger)
                s_Logger(info);

            if (s_Breaker)
            {
                s_Breaker(info);
                return false;
            }
            return true;
        }

    }

}
