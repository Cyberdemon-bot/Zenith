#include "Zenith/Compiler/Constraint.h"
#include "Zenith/Compiler/Utils.h"

namespace Zenith {

    CTType ConstraintSolver::Fresh()
    {
        uint32_t id = m_NextVar++;
        m_Parents.push_back(CTType::Var(id));
        return m_Parents.back();
    }

    void ConstraintSolver::Emit(Constraint c)
    { 
        if (c.lhs == c.rhs) return;
        m_Constraints.push_back(std::move(c)); 
    }

    void ConstraintSolver::Reset()
    {
        m_NextVar = 0;
        m_Constraints.clear();
        m_Parents.clear();
    }

    CTType ConstraintSolver::Resolve(CTType t)
    {
        CTType curr = t;
        while (curr.IsVar() && m_Parents[curr.var_id] != curr)
            curr = m_Parents[curr.var_id];
    
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

    bool ConstraintSolver::SolveImpl(Constraint& c)
    {
        switch (c.kind)
        {
            case Constraint::Kind::Eq:
            {
                if (Unify(c.lhs, c.rhs)) return true;
                c.diag.expected = Resolve(c.lhs);
                c.diag.actual = Resolve(c.rhs);
                if (c.diag.message.empty())
                    c.diag.message = Utils::FormatTypeMismatch(c.diag.diag, c.diag.expected, c.diag.actual);
                return false;
            }
        }
        return true;
    }
}