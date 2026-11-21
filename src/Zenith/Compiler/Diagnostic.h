#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include "Zenith/Compiler/CTType.h"

namespace Zenith {

    enum class Severity : uint8_t
    {
        Error,
        Warning,
        Note
    };

    enum class DiagType : uint16_t
    {
        #define DIAG(name, code, sev, tmpl) name,
        #include "Zenith/Compiler/Diagnostics.def"
        #undef DIAG
        Count
    };

    inline constexpr size_t DIAG_COUNT = static_cast<size_t>(DiagType::Count);

    struct DiagDef
    {
        uint16_t code;
        Severity severity;
        std::string_view tmpl;
    };

    inline constexpr std::array<DiagDef, DIAG_COUNT> DIAG_TABLE = {{
        #define DIAG(name, code, sev, tmpl) { code, Severity::sev, tmpl },
        #include "Zenith/Compiler/Diagnostics.def"
        #undef DIAG
    }};

    consteval bool NoDuplicateDiagCodes()
    {
        for (size_t i = 0; i < DIAG_COUNT; ++i)
            for (size_t j = i + 1; j < DIAG_COUNT; ++j)
                if (DIAG_TABLE[i].code == DIAG_TABLE[j].code) return false;
        return true;
    }

    consteval bool ValidDiagTemplates()
    {
        for (const auto& def : DIAG_TABLE)
        {
            std::string_view s = def.tmpl;
            for (size_t i = 0; i < s.size(); ++i)
            {
                if (s[i] != '{') continue;
                size_t close = s.find('}', i + 1);
                if (close == std::string_view::npos) return false;

                std::string_view key = s.substr(i + 1, close - i - 1);
                bool ok = (key == "expected" || key == "actual" ||
                           (key.size() == 1 && key[0] >= '0' && key[0] <= '3'));
                if (!ok) return false;
                i = close;
            }
        }
        return true;
    }

    static_assert(NoDuplicateDiagCodes(), "Duplicate diagnostic code in Diagnostics.def");
    static_assert(ValidDiagTemplates(),   "Invalid placeholder in Diagnostics.def template");

    constexpr const DiagDef& GetDiagDef(DiagType type) noexcept
    {
        size_t idx = static_cast<size_t>(type);
        return (idx < DIAG_COUNT) ? DIAG_TABLE[idx] : DIAG_TABLE[0];
    }

    struct Diagnostic
    {
        std::string args;
        uint32_t node = 0, token = 0;
        CTType expected = CTType::Error(), actual = CTType::Error();
        Severity severity = Severity::Error;
        DiagType diag = DiagType::UnsupportedFeature;
        bool use_node_span = true;
    };

    struct Error
    {
        DiagType type;
        std::string_view args = {};
        CTType expected = CTType::Error();
        CTType actual = CTType::Error();
    };

    std::string FormatDiagnostic(const Diagnostic& diag);
}
