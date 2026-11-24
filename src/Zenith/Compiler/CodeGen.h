#pragma once

#include "Zenith/VM/Proto.h"
#include "Zenith/Compiler/TypeChecker.h"

#include <vector>

namespace Zenith {
    
    class CodeGen
    {
    public:
        bool Generate(TypedAstView ast, Module& out);
        void Clear();

        bool HasErrors() const;
        std::span<const Diagnostic> Diagnostics() const { return m_Errors; }
    private:
        ProtoBuilder m_Builder;
        std::vector<Diagnostic> m_Errors;
    };
}