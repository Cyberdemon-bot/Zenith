#pragma once

#include "TestCase.h"
#include "Steps/AstStep.h"
#include "Steps/CompileStep.h"
#include "Steps/TypeCheckerStep.h"
#include "Steps/VmStep.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

enum class Artifact
{
    Source,
    Asm,
    Ast,
    TypedAst,
    Module,
    VmResult,
    Unknown
};

enum class Stage
{
    Parse,
    TypeCheck,
    CodeGen,
    Assemble,
    Execute
};

struct PipelinePlan
{
    Artifact start = Artifact::Unknown;
    std::vector<Stage> stages;
    std::string error;

    bool Ok() const { return error.empty(); }
};

struct PipelineResult
{
    std::map<std::string, std::string> outputs;
    std::vector<ErrorInfo> errors;
    std::vector<std::pair<std::string, float>> timings;
    std::string fatalError;
};

class Pipeline
{
public:
    static Artifact InputKindToArtifact(InputKind kind);
    static PipelinePlan Plan(const TestCase& tc);

    PipelineResult Run(const TestCase& tc, bool collectAllOutputs = false);

private:
    AstStep m_AstStep;
    TypeCheckerStep m_TypeCheckerStep;
    CompileStep m_CompileStep;
    VmStep m_VmStep;
};
