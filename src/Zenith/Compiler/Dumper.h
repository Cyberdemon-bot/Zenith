#pragma once

#include "Zenith/Compiler/Ast.h"
#include "Zenith/Compiler/CTType.h"

#include <cstdint>
#include <span>
#include <string>

namespace Zenith {

    struct ParseError;
    struct Diagnostic;

    class Dumper
    {
    public:
        static std::string DumpParseErrors(AstView ast, std::span<const ParseError> errors);
        static std::string DumpTreeAst(AstView ast, uint32_t root_idx = ERROR_NODE);
        static std::string DumpTypedAst(AstView ast, std::span<const CTType> node_types, uint32_t root_idx = ERROR_NODE);
        static std::string DumpTypeErrors(AstView ast, std::span<const Diagnostic> errors);
        static std::string DumpTypes(AstView ast, std::span<const CTType> node_types);
    };

}
