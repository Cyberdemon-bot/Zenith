#pragma once

#include "Zenith/Compiler/Ast.h"
#include "Zenith/Compiler/CTType.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace Zenith {

    struct Diagnostic;

    class Dumper
    {
    public:
        static std::string DumpDiagnostics(AstView ast, std::span<const Diagnostic> diagnostics, std::string_view phase = "Compiler");
        static std::string DumpTreeAst(AstView ast, uint32_t root_idx = ERROR_NODE);
        static std::string DumpTypedAst(AstView ast, std::span<const CTType> node_types, uint32_t root_idx = ERROR_NODE);
        static std::string DumpTypes(AstView ast, std::span<const CTType> node_types);
    };

}
