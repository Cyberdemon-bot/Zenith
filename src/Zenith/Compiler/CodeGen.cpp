#include "Zenith/Compiler/CodeGen.h"
#include "Zenith/Compiler/SignatureTable.h"

namespace Zenith {

    bool CodeGen::Generate(TypedAstView ast, Module& out)
    {
        m_Errors.Clear();
        out.Clear();
        out.Reserve(static_cast<uint16_t>(ast.signatures.Count()));

        bool is_valid = true;
        ast.signatures.ForEach([this, &out](const FuncView& view)
        {
            m_Builder.Clear();
            // compile function, flip is_valid flag if the compilation is failed

            out.Fill(view.protoId, m_Builder);
        });

        return is_valid && !HasErrors();
    }
}
