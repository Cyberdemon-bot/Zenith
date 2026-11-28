#include "VmStep.h"
#include "Diff.h"

#include "Zenith/Core/ProfileTimer.h"
#include "Zenith/VM/Assembler.h"
#include "Zenith/VM/Executor.h"

#include <charconv>
#include <sstream>

using namespace Zenith;

namespace {

    bool HasCodeOrDirective(const std::string& block)
    {
        for (const auto& raw : Diff::SplitLines(block))
        {
            std::string trimmed = Diff::Trim(raw);
            if (trimmed.empty() || trimmed.starts_with(";") || trimmed.starts_with("//")) continue;
            return true;
        }
        return false;
    }

    std::vector<std::string> SplitProtos(std::string_view asmSource)
    {
        std::vector<std::string> blocks;
        std::string current;
        bool hasExplicitProto = false;

        std::istringstream ss{ std::string(asmSource) };
        std::string line;
        while (std::getline(ss, line))
        {
            std::string trimmed = Diff::Trim(line);
            if (trimmed.starts_with(".proto"))
            {
                if (hasExplicitProto || HasCodeOrDirective(current))
                {
                    blocks.push_back(current);
                    current.clear();
                }
                else
                {
                    current.clear();
                }
                hasExplicitProto = true;
                continue;
            }
            current += line + "\n";
        }
        if (HasCodeOrDirective(current) || blocks.empty())
            blocks.push_back(current);

        return blocks;
    }

} // namespace

std::vector<VmStep::RegisterExpectation> VmStep::ParseExpectations(const std::string& expectedBlock)
{
    std::vector<RegisterExpectation> expects;
    for (const auto& raw : Diff::SplitLines(expectedBlock))
    {
        std::string line = Diff::Trim(raw);
        if (line.empty() || line.starts_with("//") || line.starts_with(";")) continue;

        if (line[0] != 'R' && line[0] != 'r') continue;
        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos) continue;

        std::string regStr = Diff::Trim(line.substr(1, eqPos - 1));
        int regIdx = 0;
        auto [ptr, ec] = std::from_chars(regStr.data(), regStr.data() + regStr.size(), regIdx);
        if (ec != std::errc{} || regIdx < 0 || regIdx > 255) continue;

        std::string right = Diff::Trim(line.substr(eqPos + 1));
        std::string type;
        size_t openParen = right.find('(');
        size_t closeParen = right.rfind(')');
        if (openParen != std::string::npos && closeParen != std::string::npos && closeParen > openParen)
        {
            type = Diff::Trim(right.substr(openParen + 1, closeParen - openParen - 1));
            right = Diff::Trim(right.substr(0, openParen));
        }

        expects.push_back({ static_cast<uint8_t>(regIdx), right, type });
    }
    return expects;
}

VmStep::AssembleResult VmStep::Assemble(std::string_view asmSource)
{
    AssembleResult res;
    auto protoBlocks = SplitProtos(asmSource);
    auto mod = CreateRef<Module>();
    uint16_t entryId = 0;

    {
        ZN_PROFILE_SCOPE(&res.timeMs);
        for (size_t i = 0; i < protoBlocks.size(); ++i)
        {
            ProtoBuilder builder;
            std::string err;
            if (!Assembler::Assemble(builder, protoBlocks[i], &err))
            {
                res.success = false;
                res.errorMessage = "Assembler error (Proto #" + std::to_string(i) + "): " + err;
                return res;
            }
            entryId = mod->Add(builder);
        }
        mod->SetEntry(entryId);
        mod->Freeze();
    }

    res.module = std::move(mod);
    res.success = true;
    return res;
}

VmStep::ExecResult VmStep::Execute(const Ref<const Module>& module, const std::string& expectedVmBlock)
{
    ExecResult res;
    if (!module || !module->HasEntry())
    {
        res.success = false;
        res.errorMessage = "VM execution error: no entry proto in module";
        return res;
    }

    uint16_t entryId = module->GetEntry();
    uint8_t entryMaxRegs = module->GetProto(entryId).MaxRegs;

    m_State.Reset();
    m_State.SetModule(module);
    m_State.Prepare(entryId);

    bool ok = true;
    try
    {
        ZN_PROFILE_SCOPE(&res.timeMs);
        ok = Executor::Execute(m_State);
    }
    catch (const std::exception& e)
    {
        res.success = false;
        res.errorMessage = "VM execution exception: " + std::string(e.what());
        return res;
    }

    if (!ok)
    {
        const RuntimeError& err = m_State.GetError();
        std::ostringstream trapOut;
        trapOut << "TRAP: " << RuntimeErrorToString(err.code)
                << " at Proto#" << err.proto_id << "[" << err.pc << "]";
        res.actualVm = trapOut.str();
        res.success = true;
        return res;
    }

    auto expects = ParseExpectations(expectedVmBlock);
    std::ostringstream vmOut;

    if (!expects.empty())
    {
        for (const auto& exp : expects)
        {
            const Value& v = m_State.Reg(exp.regIndex);
            vmOut << "R" << static_cast<int>(exp.regIndex) << " = ";

            if (exp.type == "I32")
                vmOut << v.i32 << " (I32)\n";
            else if (exp.type == "U32")
                vmOut << v.u32 << " (U32)\n";
            else if (exp.type == "F32")
                vmOut << v.f32 << " (F32)\n";
            else if (exp.type == "I64")
                vmOut << v.i64 << " (I64)\n";
            else if (exp.type == "U64")
                vmOut << v.u64 << " (U64)\n";
            else if (exp.type == "F64")
                vmOut << v.f64 << " (F64)\n";
            else if (exp.type == "BOOL")
                vmOut << (v.u64 != 0 ? "true" : "false") << " (BOOL)\n";
            else
            {
                if (exp.expectedStr.find('.') != std::string::npos)
                    vmOut << v.f32 << "\n";
                else
                    vmOut << v.i64 << "\n";
            }
        }
    }
    else
    {
        for (uint8_t i = 0; i < entryMaxRegs; ++i)
        {
            const Value& v = m_State.Reg(i);
            if (v.u64 != 0 || i == 0)
            {
                vmOut << "R" << static_cast<int>(i) << " = " << v.i32 << " (I32)\n";
            }
        }
    }

    res.actualVm = Diff::Trim(vmOut.str());
    res.success = true;
    return res;
}
