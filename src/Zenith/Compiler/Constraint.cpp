#include "Zenith/Compiler/Constraint.h"
#include "Zenith/Compiler/Utils.h"
#include "Zenith/Core/Assert.h"

namespace Zenith {

    CTType ConstraintSolver::Fresh()
    {
        uint32_t id = m_NextVar++;
        m_Parents.push_back(CTType::Var(id));
        return m_Parents.back();
    }

    void ConstraintSolver::Emit(Constraint c)
    {
        if (c.kind == Constraint::Kind::Eq && c.lhs == c.rhs) return;
        m_Constraints.push_back(std::move(c));
    }

    void ConstraintSolver::Reset()
    {
        m_NextVar = 0;
        m_Constraints.clear();
        m_Parents.clear();
        m_Done.clear();
        m_Failed.clear();
    }

    CTType ConstraintSolver::Resolve(CTType t)
    {
        CTType curr = t;
        while (curr.IsVar())
        {
            ZN_CORE_ASSERT(curr.var_id < m_Parents.size(), "Type variable id out of bounds");
            if (m_Parents[curr.var_id] == curr) break;
            curr = m_Parents[curr.var_id];
        }

        CTType temp = t;
        while (temp.IsVar() && m_Parents[temp.var_id] != temp)
        {
            CTType next = m_Parents[temp.var_id];
            m_Parents[temp.var_id] = curr;
            temp = next;
        }

        return curr;
    }

    bool ConstraintSolver::Unify(CTType a, CTType b)
    {
        a = Resolve(a);
        b = Resolve(b);

        if (a == b) return true;
        if (a.IsVar()) { m_Parents[a.var_id] = b; return true; }
        if (b.IsVar()) { m_Parents[b.var_id] = a; return true; }

        return a == CTType::Error() || b == CTType::Error();
    }

    void ConstraintSolver::SolveImpl()
    {
        const uint32_t count = static_cast<uint32_t>(m_Constraints.size());
        m_Done.assign(count, false);
        m_Failed.clear();

        // resolve all eq constraint
        for (uint32_t i = 0; i < count; ++i)
        {
            Constraint& c = m_Constraints[i];
            if (c.kind != Constraint::Kind::Eq) continue;

            m_Done[i] = true;
            if (!Unify(c.lhs, c.rhs))
            {
                c.diag.expected = Resolve(c.lhs);
                c.diag.actual = Resolve(c.rhs);
                m_Failed.push_back(i);
            }
        }

        // fixed-point propagation and numeric literal fallback
        bool outer_progress = true;
        while (outer_progress)
        {
            bool changed = true; outer_progress = false;
            while (changed)
            {
                changed = false;
                for (uint32_t i = 0; i < count; ++i)
                {
                    if (m_Done[i]) continue;
                    Constraint& c = m_Constraints[i];

                    switch (c.kind)
                    {
                        case Constraint::Kind::UnaryOp: changed |= SolveUnaryOp(i);  break;
                        case Constraint::Kind::BinaryOp: changed |= SolveBinaryOp(i); break;
                        case Constraint::Kind::Cast: changed |= SolveCast(i);     break;
                        case Constraint::Kind::IntLit:
                        case Constraint::Kind::FloatLit: changed |= SolveLiteral(i);  break;
                        default:
                            m_Done[i] = true;
                            break;
                    }
                }
            }

            // Default one unresolved numeric literal per outer pass, then re-propagate
            // so linked constraints converge before defaulting remaining literals.
            for (uint32_t i = 0; i < count; ++i)
            {
                if (m_Done[i]) continue;
                Constraint& c = m_Constraints[i];
                if (c.kind != Constraint::Kind::IntLit && c.kind != Constraint::Kind::FloatLit) continue;

                CTType target = Resolve(c.lhs);
                if (target.IsVar())
                {
                    CTType def = (c.kind == Constraint::Kind::IntLit) ? CTType::I64() : CTType::F64();
                    Unify(target, def);
                    outer_progress = true;
                    break;
                }
            }
        }

        for (uint32_t i = 0; i < count; ++i)
        {
            if (m_Done[i]) continue;
            Constraint& c = m_Constraints[i];
            m_Done[i] = true;
            c.diag.diag = DiagType::CannotInferType;
            m_Failed.push_back(i);
        }
    }

    bool ConstraintSolver::SolveUnaryOp(uint32_t idx)
    {
        Constraint& c = m_Constraints[idx];
        CTType opnd = Resolve(c.lhs);
        if (opnd == CTType::Error()) { m_Done[idx] = true; return false; }

        if (!opnd.IsVar())
        {
            m_Done[idx] = true;
            CTType res_type = Utils::CheckUnaryOp(c.op, opnd);
            if (!res_type)
            {
                c.diag.diag = (c.op == Operator::Not || c.op == Operator::Neg)
                    ? DiagType::OperandTypeMismatch
                    : DiagType::InvalidUnaryOp;
                c.diag.args = std::string(OperatorToString(c.op));
                c.diag.actual = opnd;
                m_Failed.push_back(idx);
            }
            else if (!Unify(c.result, res_type))
            {
                c.diag.expected = Resolve(c.result);
                c.diag.actual = res_type;
                m_Failed.push_back(idx);
            }
            return true;
        }

        if (c.op == Operator::Not)
        {
            // '!' requires bool operand and yields bool
            m_Done[idx] = true;
            Unify(opnd, CTType::Bool());
            Unify(c.result, CTType::Bool());
            return true;
        }

        if (c.op == Operator::Neg)
        {
            CTType res = Resolve(c.result);
            if (!res.IsVar() && res != CTType::Error())
            {
                Unify(opnd, res);
                return true;
            }
        }

        return false;
    }

    bool ConstraintSolver::SolveBinaryOp(uint32_t idx)
    {
        Constraint& c = m_Constraints[idx];

        if (!Unify(c.lhs, c.rhs))
        {
            m_Done[idx] = true;
            c.diag.diag = DiagType::InvalidBinaryOp;
            c.diag.args = std::string(OperatorToString(c.op));
            c.diag.expected = Resolve(c.lhs);
            c.diag.actual = Resolve(c.rhs);
            m_Failed.push_back(idx);
            return false;
        }

        CTType l = Resolve(c.lhs);
        CTType r = Resolve(c.rhs);
        if (l == CTType::Error() || r == CTType::Error()) { m_Done[idx] = true; return false; }

        if (!l.IsVar() && !r.IsVar())
        {
            m_Done[idx] = true;
            CTType res_type = Utils::CheckBinaryOp(c.op, l, r);
            if (!res_type)
            {
                c.diag.diag = (l != r) ? DiagType::InvalidBinaryOp : DiagType::OperandTypeMismatch;
                c.diag.args = std::string(OperatorToString(c.op));
                c.diag.expected = l;
                c.diag.actual = r;
                m_Failed.push_back(idx);
            }
            else if (!Unify(c.result, res_type))
            {
                c.diag.expected = Resolve(c.result);
                c.diag.actual = res_type;
                m_Failed.push_back(idx);
            }
            return true;
        }

        if (Utils::IsArithmeticOp(c.op))
        {
            CTType res = Resolve(c.result);
            if (!res.IsVar() && res != CTType::Error())
            {
                Unify(l, res);
                Unify(r, res);
                return true;
            }
        }

        return false;
    }

    bool ConstraintSolver::SolveCast(uint32_t idx)
    {
        Constraint& c = m_Constraints[idx];
        CTType src = Resolve(c.lhs);
        CTType dst = Resolve(c.rhs);
        if (src == CTType::Error() || dst == CTType::Error()) { m_Done[idx] = true; return false; }

        if (!src.IsVar() && !dst.IsVar())
        {
            m_Done[idx] = true;
            if (!Utils::ValidatePrimitiveCast(src, dst))
            {
                c.diag.diag = DiagType::CoerceFailure;
                c.diag.expected = dst;
                c.diag.actual = src;
                m_Failed.push_back(idx);
            }
            return true;
        }

        return false;
    }

    bool ConstraintSolver::SolveLiteral(uint32_t idx)
    {
        Constraint& c = m_Constraints[idx];
        CTType target = Resolve(c.lhs);
        if (target == CTType::Error()) { m_Done[idx] = true; return false; }

        if (!target.IsVar())
        {
            m_Done[idx] = true;
            bool is_ok = (c.kind == Constraint::Kind::IntLit)
                ? Utils::CheckIntRange(c.lit.i64, target)
                : Utils::CheckFloatRange(c.lit.f64, target);
            if (!is_ok)
            {
                bool is_expected_kind = (c.kind == Constraint::Kind::IntLit)
                    ? Utils::IsInteger(target)
                    : Utils::IsFloat(target);
                if (is_expected_kind)
                    c.diag.diag = DiagType::LiteralOutOfRange;
                c.diag.expected = target;
                c.diag.actual = (c.kind == Constraint::Kind::IntLit) ? CTType::I64() : CTType::F64();
                m_Failed.push_back(idx);
            }
            return true;
        }

        return false;
    }
}
