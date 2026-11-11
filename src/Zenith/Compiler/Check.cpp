#include "Zenith/Compiler/Compiler.h"
#include "Zenith/Core/Assert.h"
#include "Zenith/Compiler/Node.h"
#include "Zenith/Compiler/TypeChecker.h"

namespace Zenith {

    bool TypeChecker::CheckExpr(uint32_t node, CTType expected, Error err)
    {
        const auto& n = m_Ast.nodes[node];
        CTType actual = CTType::Error;
        switch (n.type)
        {
            case NodeType::LabeledExpr:
            {

            }
            case NodeType::BlockExpr:
            {
                PushTarget(ControlTarget{node, "", expected, false});
                PushScope();

                const uint32_t count = ListCount(node);
                for (size_t i = 0; i < count; ++i)
                {
                    uint32_t stmt = ListItem(node, i);
                    if(!CheckStmt(stmt))
                    {
                        // add error
                        return false;
                    }
                }
                auto target = GetCurrentTarget(); // snapshot

                PopScope();
                PopTarget();

                if (expected != CTType::Unit && !target.has_break)
                {
                    AddError(node, {DiagType::BlockMissingBreak, "Block expected a value but has no break statement"});
                    return false;
                }
            }   
            case NodeType::ForExpr:
            {

            }
            case NodeType::IfExpr:
            {

            }
            default: actual = InferExpr(node);
        }

        if (actual == expected) return true;

        AddError(node, err);
        return false;
    }

    bool TypeChecker::CheckStmt(uint32_t node)
    {
        return false;
    }
}