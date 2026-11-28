#include "Pipeline.h"

#include <algorithm>
#include <array>
#include <queue>
#include <string_view>

namespace {

    struct StepEdge
    {
        Artifact from;
        Artifact to;
        Stage stage;
    };

    constexpr std::array<StepEdge, 5> kPipelineEdges = {{
        { Artifact::Source,   Artifact::Ast,      Stage::Parse     },
        { Artifact::Ast,      Artifact::TypedAst, Stage::TypeCheck },
        { Artifact::TypedAst, Artifact::Module,   Stage::CodeGen   },
        { Artifact::Asm,      Artifact::Module,   Stage::Assemble  },
        { Artifact::Module,   Artifact::VmResult, Stage::Execute   },
    }};

    std::string_view ArtifactName(Artifact a)
    {
        switch (a)
        {
            case Artifact::Source:   return "SOURCE";
            case Artifact::Asm:      return "ASM";
            case Artifact::Ast:      return "AST";
            case Artifact::TypedAst: return "TYPES";
            case Artifact::Module:   return "MODULE";
            case Artifact::VmResult: return "VM";
            default:                 return "UNKNOWN";
        }
    }

    // BFS shortest path from `start` to `goal` over kPipelineEdges.
    bool FindPath(Artifact start, Artifact goal, std::vector<Stage>& outStages)
    {
        outStages.clear();
        if (start == goal) return true;

        constexpr size_t kCount = static_cast<size_t>(Artifact::Unknown) + 1;
        std::array<bool, kCount> visited{};
        std::array<Artifact, kCount> parentArtifact{};
        std::array<Stage, kCount> parentStage{};

        std::queue<Artifact> q;
        visited[static_cast<size_t>(start)] = true;
        q.push(start);

        bool found = false;
        while (!q.empty())
        {
            Artifact cur = q.front();
            q.pop();
            if (cur == goal)
            {
                found = true;
                break;
            }
            for (const auto& edge : kPipelineEdges)
            {
                if (edge.from != cur) continue;
                size_t nextIdx = static_cast<size_t>(edge.to);
                if (visited[nextIdx]) continue;
                visited[nextIdx] = true;
                parentArtifact[nextIdx] = cur;
                parentStage[nextIdx] = edge.stage;
                q.push(edge.to);
            }
        }

        if (!found) return false;

        Artifact cur = goal;
        while (cur != start)
        {
            size_t idx = static_cast<size_t>(cur);
            outStages.push_back(parentStage[idx]);
            cur = parentArtifact[idx];
        }
        std::reverse(outStages.begin(), outStages.end());
        return true;
    }

} // namespace

Artifact Pipeline::InputKindToArtifact(InputKind kind)
{
    switch (kind)
    {
        case InputKind::Source: return Artifact::Source;
        case InputKind::Asm:    return Artifact::Asm;
        default:                return Artifact::Unknown;
    }
}

PipelinePlan Pipeline::Plan(const TestCase& tc)
{
    PipelinePlan plan;
    plan.start = InputKindToArtifact(tc.kind);
    if (plan.start == Artifact::Unknown)
    {
        plan.error = "Unsupported input kind";
        return plan;
    }

    std::vector<Artifact> targets;
    if (tc.hasAst)    targets.push_back(Artifact::Ast);
    if (tc.hasTypes)  targets.push_back(Artifact::TypedAst);
    if (tc.hasDisasm) targets.push_back(Artifact::Module);
    if (tc.hasVm)     targets.push_back(Artifact::VmResult);

    if (targets.empty())
    {
        if (tc.hasErrors)
        {
            // Run along the compile path up to Module; execution halts at the first stage with errors.
            if (!FindPath(plan.start, Artifact::Module, plan.stages))
                plan.error = "No compile path available for error expectation";
            return plan;
        }

        // No expectations yet (e.g. --update on a fresh test file): pick natural default target.
        Artifact defaultTarget = (plan.start == Artifact::Asm) ? Artifact::VmResult : Artifact::Ast;
        FindPath(plan.start, defaultTarget, plan.stages);
        return plan;
    }

    std::vector<Stage> longestPath;
    for (Artifact target : targets)
    {
        std::vector<Stage> path;
        if (!FindPath(plan.start, target, path))
        {
            plan.error = "Incompatible expectation: cannot reach "
                       + std::string(ArtifactName(target))
                       + " from input " + std::string(ArtifactName(plan.start));
            return plan;
        }
        if (path.size() > longestPath.size())
            longestPath = std::move(path);
    }

    plan.stages = std::move(longestPath);
    return plan;
}

PipelineResult Pipeline::Run(const TestCase& tc, bool collectAllOutputs)
{
    PipelineResult result;

    PipelinePlan plan = Plan(tc);
    if (!plan.Ok())
    {
        result.fatalError = plan.error;
        return result;
    }

    Zenith::AstView ast{};
    Zenith::TypedAstView typed{};
    Zenith::Ref<const Zenith::Module> module;

    for (Stage stage : plan.stages)
    {
        switch (stage)
        {
            case Stage::Parse:
            {
                auto res = m_AstStep.Run(tc.inputSource);
                result.timings.push_back({ "Parse", res.timeMs });
                ast = res.ast;

                if (tc.hasAst || (collectAllOutputs && !tc.hasTypes && !tc.hasDisasm && !tc.hasVm))
                {
                    float dumpMs = 0.0f;
                    result.outputs[std::string(TestCaseLoader::kExpectedAstMarker)] =
                        AstStep::Dump(ast, &dumpMs);
                    result.timings.push_back({ "Dump", dumpMs });
                }

                if (res.hasErrors)
                {
                    result.errors = std::move(res.errors);
                    return result;
                }
                break;
            }

            case Stage::TypeCheck:
            {
                auto res = m_TypeCheckerStep.Run(ast);
                result.timings.push_back({ "Check", res.timeMs });
                typed = res.typed;

                if (tc.hasTypes || (collectAllOutputs && !tc.hasDisasm && !tc.hasVm))
                {
                    float dumpMs = 0.0f;
                    result.outputs[std::string(TestCaseLoader::kExpectedTypesMarker)] =
                        TypeCheckerStep::Dump(typed, &dumpMs);
                    result.timings.push_back({ "Dump", dumpMs });
                }

                if (res.hasErrors)
                {
                    result.errors = std::move(res.errors);
                    return result;
                }
                break;
            }

            case Stage::CodeGen:
            {
                auto res = m_CompileStep.Run(typed);
                result.timings.push_back({ "CodeGen", res.timeMs });
                if (res.hasErrors)
                {
                    result.errors = std::move(res.errors);
                    return result;
                }
                module = std::move(res.module);

                if (tc.hasDisasm || (collectAllOutputs && !tc.hasVm))
                {
                    float disasmMs = 0.0f;
                    result.outputs[std::string(TestCaseLoader::kExpectedDisasmMarker)] =
                        CompileStep::DumpDisasm(*module, &disasmMs);
                    result.timings.push_back({ "Disasm", disasmMs });
                }
                break;
            }

            case Stage::Assemble:
            {
                auto res = m_VmStep.Assemble(tc.inputSource);
                result.timings.push_back({ "Asm", res.timeMs });
                if (!res.success)
                {
                    result.fatalError = res.errorMessage;
                    return result;
                }
                module = std::move(res.module);

                if (tc.hasDisasm || (collectAllOutputs && !tc.hasVm))
                {
                    float disasmMs = 0.0f;
                    result.outputs[std::string(TestCaseLoader::kExpectedDisasmMarker)] =
                        CompileStep::DumpDisasm(*module, &disasmMs);
                    result.timings.push_back({ "Disasm", disasmMs });
                }
                break;
            }

            case Stage::Execute:
            {
                auto res = m_VmStep.Execute(module, tc.expectedVm);
                result.timings.push_back({ "Exec", res.timeMs });
                if (!res.success)
                {
                    result.fatalError = res.errorMessage;
                    return result;
                }
                result.outputs[std::string(TestCaseLoader::kExpectedVmMarker)] = res.actualVm;
                break;
            }
        }
    }

    return result;
}
