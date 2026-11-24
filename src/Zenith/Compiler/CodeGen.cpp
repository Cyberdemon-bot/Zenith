#include "Zenith/Compiler/CodeGen.h"
#include "Zenith/Compiler/SignatureTable.h"

namespace Zenith {

    bool CodeGen::HasErrors() const
    {
        for (const auto& d : m_Errors)
            if (d.severity == Severity::Error) return true;
        return false;
    }

    void CodeGen::Clear()
    {
        m_Builder.Clear();
        m_Errors.clear();
    }

    bool CodeGen::Generate(TypedAstView ast, Module& out)
    {
        Clear();
        out.Clear();
        out.Reserve(ast.signatures.Count());

        bool is_valid = true;
        ast.signatures.ForEach([&](const FuncView& view)
        {
            m_Builder.Clear();
            // compile function, flip is_valid flag if the compilation is failed

            out.Fill(view.protoId, m_Builder);
        });

        return is_valid;
    }
}