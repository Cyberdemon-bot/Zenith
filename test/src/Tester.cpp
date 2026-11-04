#include "Tester.h"
#include "Diff.h"
#include "Steps/AstStep.h"
#include "Steps/VmStep.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>

std::vector<TestCase> Tester::LoadCases(const fs::path& dir, const std::string& filter)
{
    std::vector<TestCase> cases;
    for (const auto& entry : fs::recursive_directory_iterator(dir))
    {
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension();
        if (ext != ".test") continue;

        TestCase tc = TestCaseLoader::LoadCase(entry.path(), dir);
        if (!filter.empty() && tc.name.find(filter) == std::string::npos) continue;
        cases.push_back(std::move(tc));
    }
    std::sort(cases.begin(), cases.end(), [](const TestCase& a, const TestCase& b) { return a.name < b.name; });
    return cases;
}

TestResult Tester::RunCase(const TestCase& tc, const Options& opt)
{
    if (!tc.loadError.empty())
    {
        std::cout << Diff::Paint("31", "[FAIL]") << " " << tc.name << "\n  " << tc.loadError << "\n";
        return TestResult::Fail;
    }
    if (Diff::Trim(tc.inputSource).empty())
    {
        std::cout << Diff::Paint("31", "[FAIL]") << " " << tc.name << "\n  No input source before expected markers\n";
        return TestResult::Fail;
    }

    std::map<std::string, std::string> actualOutputs;
    std::vector<ErrorInfo> actualErrors;
    bool allMatched = true;
    std::string pipelineError;

    try
    {
        if (tc.kind == InputKind::Source)
        {
            auto astResult = AstStep::Run(tc.inputSource);
            actualOutputs[std::string(TestCaseLoader::kExpectedAstMarker)] = astResult.astTree;
            actualErrors = astResult.errors;

            if (tc.hasAst)
            {
                if (astResult.astTree != tc.expectedAst)
                    allMatched = false;
            }

            if (tc.hasErrors || !actualErrors.empty())
            {
                std::string expectedErrStr = TestCaseLoader::RenderExpected(tc.expectedErrors, actualErrors);
                std::string actualErrStr = TestCaseLoader::RenderErrors(actualErrors);
                if (expectedErrStr != actualErrStr)
                    allMatched = false;
            }
        }
        else if (tc.kind == InputKind::Asm)
        {
            auto vmResult = VmStep::RunAsm(tc.inputSource, tc.expectedVm);
            if (!vmResult.success)
            {
                pipelineError = vmResult.errorMessage;
                allMatched = false;
            }
            else
            {
                actualOutputs[std::string(TestCaseLoader::kExpectedVmMarker)] = vmResult.actualVm;
                actualOutputs[std::string(TestCaseLoader::kExpectedDisasmMarker)] = vmResult.actualDisasm;

                if (tc.hasVm)
                {
                    if (vmResult.actualVm != tc.expectedVm)
                        allMatched = false;
                }
                if (tc.hasDisasm)
                {
                    if (vmResult.actualDisasm != tc.expectedDisasm)
                        allMatched = false;
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        pipelineError = "Exception in test pipeline: " + std::string(e.what());
        allMatched = false;
    }

    if (opt.dump)
    {
        std::cout << "--- " << tc.name << " ---\n";
        if (!actualErrors.empty())
            std::cout << TestCaseLoader::RenderErrors(actualErrors) << "\n\n";
        for (const auto& [tag, content] : actualOutputs)
        {
            std::cout << tag << "\n" << content << "\n\n";
        }
    }

    bool hasAnyExpectation = tc.hasAst || tc.hasErrors || tc.hasVm || tc.hasDisasm;

    if (!hasAnyExpectation && !opt.update)
    {
        std::cout << Diff::Paint("31", "[FAIL]") << " " << tc.name
                  << "\n  No expectations found; add expected section or run with --update\n";
        return TestResult::Fail;
    }

    if (!pipelineError.empty())
    {
        std::cout << Diff::Paint("31", "[FAIL]") << " " << tc.name << "\n  " << pipelineError << "\n";
        return TestResult::Fail;
    }

    if (hasAnyExpectation && allMatched)
    {
        std::cout << Diff::Paint("32", "[PASS]") << " " << tc.name << "\n";
        return TestResult::Pass;
    }

    if (opt.update)
    {
        std::string updated = TestCaseLoader::UpdatedContent(tc, actualOutputs, actualErrors);
        std::ofstream file(tc.path, std::ios::binary);
        if (!file)
        {
            std::cout << Diff::Paint("31", "[FAIL]") << " " << tc.name << "\n  Cannot write to " << tc.path << "\n";
            return TestResult::Fail;
        }
        file << updated;
        std::cout << Diff::Paint("33", "[UPDATED]") << " " << tc.name << "\n";
        return TestResult::Updated;
    }

    std::cout << Diff::Paint("31", "[FAIL]") << " " << tc.name << "\n";
    if (tc.hasErrors || !actualErrors.empty())
    {
        std::string expectedErrStr = TestCaseLoader::RenderExpected(tc.expectedErrors, actualErrors);
        std::string actualErrStr = TestCaseLoader::RenderErrors(actualErrors);
        if (expectedErrStr != actualErrStr)
        {
            std::cout << "  Errors mismatch:\n";
            Diff::PrintDiff(expectedErrStr, actualErrStr);
        }
    }
    if (tc.hasAst)
    {
        auto it = actualOutputs.find(std::string(TestCaseLoader::kExpectedAstMarker));
        std::string actualAst = (it != actualOutputs.end()) ? it->second : "";
        if (actualAst != tc.expectedAst)
        {
            std::cout << "  AST mismatch:\n";
            Diff::PrintDiff(tc.expectedAst, actualAst);
        }
    }
    if (tc.hasVm)
    {
        auto it = actualOutputs.find(std::string(TestCaseLoader::kExpectedVmMarker));
        std::string actualVm = (it != actualOutputs.end()) ? it->second : "";
        if (actualVm != tc.expectedVm)
        {
            std::cout << "  VM mismatch:\n";
            Diff::PrintDiff(tc.expectedVm, actualVm);
        }
    }
    if (tc.hasDisasm)
    {
        auto it = actualOutputs.find(std::string(TestCaseLoader::kExpectedDisasmMarker));
        std::string actualDisasm = (it != actualOutputs.end()) ? it->second : "";
        if (actualDisasm != tc.expectedDisasm)
        {
            std::cout << "  Disasm mismatch:\n";
            Diff::PrintDiff(tc.expectedDisasm, actualDisasm);
        }
    }

    return TestResult::Fail;
}

int Tester::Run(const Options& opt)
{
    if (!fs::is_directory(opt.dir))
    {
        std::cerr << "Test directory not found: " << opt.dir << " (cwd: " << fs::current_path().string() << ")\n";
        return 2;
    }

    std::vector<TestCase> cases = LoadCases(opt.dir, opt.filter);
    if (cases.empty())
    {
        std::cerr << "No test cases found in " << opt.dir << "\n";
        return 2;
    }

    std::cout << "Running " << cases.size() << " test cases" << (opt.update ? " [UPDATE MODE]" : "") << "\n\n";

    int passed = 0, updated = 0;
    std::vector<std::string> failed;
    for (const auto& tc : cases)
    {
        switch (RunCase(tc, opt))
        {
            case TestResult::Pass:    ++passed; break;
            case TestResult::Updated: ++updated; break;
            case TestResult::Fail:    failed.push_back(tc.name); break;
        }
    }

    std::cout << "\n===============================\n"
              << "Total: " << cases.size() << " | Passed: " << passed
              << " | Failed: " << failed.size() << " | Updated: " << updated << "\n";
    for (const auto& name : failed) std::cout << "  failed: " << name << "\n";
    std::cout << "===============================\n";

    return failed.empty() ? 0 : 1;
}

void Tester::PrintUsage()
{
    std::cout
        << "Usage: test [dir] [options]\n"
        << "  dir               directory with *.test files, searched recursively (default: test/cases)\n"
        << "  -f, --filter STR  run only tests whose name contains STR\n"
        << "  -u, --update      rewrite expectations with the actual output\n"
        << "  -d, --dump        print the actual errors and AST/VM of every test\n"
        << "      --no-color    disable colored output\n"
        << "  -h, --help        show this help\n";
}
