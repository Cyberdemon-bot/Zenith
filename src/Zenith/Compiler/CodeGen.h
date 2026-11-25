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
        ProtoBuilder m_Builder;
        ErrorStack m_Errors;
    };
}
