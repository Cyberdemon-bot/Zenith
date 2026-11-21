#pragma once

#include <concepts>
#include <cstdint>
#include <utility>
#include <vector>

#include "Zenith/Compiler/CTType.h"
#include "Zenith/Compiler/Diagnostic.h"
#include "Zenith/Compiler/Node.h"

namespace Zenith {

    struct Constraint
    {
        enum class Kind : uint8_t
        {
            Eq,
            IntLit,
            FloatLit,
            UnaryOp,
            BinaryOp,
            Cast
        };

        Kind kind;
        CTType lhs = CTType::Error();
        CTType rhs = CTType::Error();
        CTType result = CTType::Error();
        Operator op = Operator::Add;
        union
        {
            int64_t i64;
            double f64;
        } lit { .i64 = 0 };
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
        void Solve(Fn&& on_error)
        {
            SolveImpl();
            for (uint32_t idx : m_Failed)
                on_error(std::move(m_Constraints[idx].diag));
        }

    private:
        void SolveImpl();
        bool SolveUnaryOp(uint32_t idx);
        bool SolveBinaryOp(uint32_t idx);
        bool SolveCast(uint32_t idx);
        bool SolveLiteral(uint32_t idx);

        uint32_t m_NextVar = 0;
        std::vector<Constraint> m_Constraints;
        std::vector<CTType> m_Parents;
        std::vector<bool> m_Done;
        std::vector<uint32_t> m_Failed;
    };
}
