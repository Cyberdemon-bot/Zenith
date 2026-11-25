#pragma once

#include "Zenith/VM/Proto.h"
#include "Zenith/Compiler/TypeChecker.h"

namespace Zenith {
    
    class CodeGen
    {
    public:
        bool Generate(TypedAstView ast, Module& out);

        bool HasErrors() const { return m_Errors.HasErrors(); }
        std::span<const Diagnostic> Diagnostics() const { return m_Errors.View(); }
    private:
        TypedAstView m_Typed{};
        ProtoBuilder m_Builder;
        ErrorStack m_Errors;

        std::string_view GetName(uint32_t node) const;
        uint32_t ListCount(uint32_t list_node) const;
        uint32_t ListItem(uint32_t list_node, uint32_t i) const;
    };
}
