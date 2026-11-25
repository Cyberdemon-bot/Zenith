#include "Zenith/Compiler/CodeGen.h"
#include "Zenith/Compiler/Utils.h"
#include "Zenith/Compiler/SignatureTable.h"

namespace Zenith {

    bool CodeGen::Generate(TypedAstView ast, Module& out)
    {
        ZN_CORE_ASSERT(ast.signatures != nullptr, "TypedAstView signatures cannot be null");
        m_Errors.Clear(); out.Clear();
        m_Typed = ast;
        out.Reserve(static_cast<uint16_t>(m_Typed.signatures->Count()));

        bool is_valid = true;
        m_Typed.signatures->ForEach([this, &out](const FuncView& view)
        {
            m_Builder.Clear();
            // compile function, flip is_valid flag if the compilation is failed

            out.Fill(view.protoId, m_Builder);
        });

        return is_valid && !HasErrors();
    }

    std::string_view CodeGen::GetName(uint32_t node) const
    {
        std::string_view name;
        [[maybe_unused]] bool ok = Utils::GetName(m_Typed.ast, node, name);
        ZN_CORE_ASSERT(ok, "GetName: expected valid Identifier or Label node");
        return name;
    }

    uint32_t CodeGen::ListCount(uint32_t list_node) const
    {
        uint32_t count = 0;
        [[maybe_unused]] bool ok = Utils::ListCount(m_Typed.ast, list_node, count);
        ZN_CORE_ASSERT(ok, "ListCount: invalid list node");
        return count;
    }

    uint32_t CodeGen::ListItem(uint32_t list_node, uint32_t i) const
    {
        uint32_t item = 0;
        [[maybe_unused]] bool ok = Utils::ListItem(m_Typed.ast, list_node, i, item);
        ZN_CORE_ASSERT(ok, "ListItem: invalid list node or index out of bounds");
        return item;
    }
}
