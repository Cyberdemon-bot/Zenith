#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include "Zenith/Compiler/CTType.h"
#include "Zenith/Compiler/Diagnostic.h"

namespace Zenith {

    struct Constraint
    {
        enum class Kind { Eq };

        Kind kind;
        CTType lhs, rhs;
        Diagnostic diag;
    };

    class ConstraintSolver
    {
    public:
        CTType Fresh() { return CTType::Var(m_NextVar++); }
        void Emit(Constraint c) { m_Constraints.push_back(std::move(c)); }

        
    private:
        uint32_t m_NextVar = 0;
        std::vector<Constraint> m_Constraints;
        std::vector<CTType> m_Bindings; // m_NextVar ​​increase linearly
    };
}