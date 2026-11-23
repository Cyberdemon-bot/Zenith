#pragma once

#include "Zenith/VM/Proto.h"
#include "Zenith/Compiler/TypeChecker.h"

#include <optional>

namespace Zenith {
    
    class CodeGen
    {
    public:
        std::optional<Module> Generate(TypedAstView ast);
    private:
    };
}