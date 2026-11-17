#pragma once

#include <concepts>
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
        void Reset();
        void Emit(Constraint c);

        CTType Fresh();
        CTType Resolve(CTType t);
        bool Unify(CTType a, CTType b);

        template<std::invocable<Diagnostic&&> Fn>
        void Solve(Fn&& onError)
        {
            for (Constraint& c : m_Constraints)
                if (!SolveImpl(c))
                    onError(std::move(c.diag));
        }
        
    private:
        bool SolveImpl(Constraint& c);

        uint32_t m_NextVar = 0;
        std::vector<Constraint> m_Constraints;
        std::vector<CTType> m_Parents; // m_NextVar ​​increase linearly
    };
}