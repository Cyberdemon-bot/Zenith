#include "Zenith/Compiler/Diagnostic.h"
#include "Zenith/Compiler/Utils.h"

namespace Zenith {

    std::string FormatDiagnostic(const Diagnostic& diag)
    {
        std::string_view tmpl = GetDiagDef(diag.diag).tmpl;

        std::string_view args[4]{};
        size_t arg_count = 0;
        if (!diag.args.empty())
        {
            size_t start = 0;
            while (arg_count < 4)
            {
                size_t pos = diag.args.find('\x1f', start);
                if (pos == std::string::npos)
                {
                    args[arg_count++] = std::string_view(diag.args).substr(start);
                    break;
                }
                args[arg_count++] = std::string_view(diag.args).substr(start, pos - start);
                start = pos + 1;
            }
        }

        std::string out;
        out.reserve(tmpl.size() + 32);
        for (size_t i = 0; i < tmpl.size(); ++i)
        {
            if (tmpl[i] == '{')
            {
                size_t close = tmpl.find('}', i + 1);
                if (close != std::string_view::npos)
                {
                    std::string_view key = tmpl.substr(i + 1, close - i - 1);
                    if (key == "expected")      out += Utils::TypeToString(diag.expected);
                    else if (key == "actual")   out += Utils::TypeToString(diag.actual);
                    else if (key.size() == 1 && key[0] >= '0' && key[0] <= '3')
                    {
                        size_t idx = static_cast<size_t>(key[0] - '0');
                        if (idx < arg_count) out += args[idx];
                    }
                    i = close;
                    continue;
                }
            }
            out += tmpl[i];
        }
        return out;
    }
}