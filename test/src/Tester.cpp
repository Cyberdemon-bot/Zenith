#include "Tester.h"
#include "Diff.h"
#include "Pipeline.h"
#include "Zenith/Core/ProfileTimer.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace {

    std::string FormatTimings(float caseTimeMs, const std::vector<std::pair<std::string, float>>& timings)
    {
        std::ostringstream os;
        os << std::fixed << std::setprecision(2) << " (" << caseTimeMs << "ms)";
        if (!timings.empty())
        {
            os << " [";
            for (size_t i = 0; i < timings.size(); ++i)
            {
                if (i > 0) os << ", ";
                os << timings[i].first << ": " << timings[i].second << "ms";
            }
            os << "]";
        }
        return os.str();
    }

    bool CheckSection(
        bool hasExpected,
        const std::string& expected,
        const std::map<std::string, std::string>& outputs,
        std::string_view marker)
    {
        if (!hasExpected) return true;
        auto it = outputs.find(std::string(marker));
        std::string actual = (it != outputs.end()) ? it->second : "";
        return actual == expected;
    }

    void PrintSectionDiff(
        bool hasExpected,
        const std::string& expected,
        const std::map<std::string, std::string>& outputs,
        std::string_view marker,
        std::string_view label)
    {
        if (!hasExpected) return;
        auto it = outputs.find(std::string(marker));
        std::string actual = (it != outputs.end()) ? it->second : "";
        if (actual != expected)
        {
            std::cout << "  " << label << " mismatch:\n";
            Diff::PrintDiff(expected, actual);
        }
    }

} // namespace

std::vector<TestCase> Tester::LoadCases(const fs::path& dir, const std::string& filter)
{
    std::vector<TestCase> cases;
    for (const auto& entry : fs::recursive_directory_iterator(dir))
    {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".test") continue;

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

    static Pipeline pipeline;
    PipelineResult res;
    float caseTimeMs = 0.0f;

    {
        ZN_PROFILE_SCOPE(&caseTimeMs);
        try
        {
            res = pipeline.Run(tc, opt.update || opt.dump);
        }
        catch (const std::exception& e)
        {
            res.fatalError = "Exception in test pipeline: " + std::string(e.what());
        }
    }

    std::string timeStr = FormatTimings(caseTimeMs, res.timings);

    if (opt.dump)
    {
        std::cout << "--- " << tc.name << " ---\n";
        std::cout << "Execution Time: " << std::fixed << std::setprecision(2) << caseTimeMs << "ms\n";
        for (const auto& [stageName, ms] : res.timings)
            std::cout << "  - " << stageName << ": " << ms << "ms\n";
        std::cout << "\n";
        if (!res.errors.empty())
            std::cout << TestCaseLoader::RenderErrors(res.errors) << "\n\n";
        for (const auto& [tag, content] : res.outputs)
            std::cout << tag << "\n" << content << "\n\n";
    }

    bool hasAnyExpectation = tc.hasAst || tc.hasTypes || tc.hasErrors || tc.hasVm || tc.hasDisasm;

    if (!hasAnyExpectation && !opt.update)
    {
        std::cout << Diff::Paint("31", "[FAIL]") << " " << tc.name << timeStr
                  << "\n  No expectations found; add expected section or run with --update\n";
        return TestResult::Fail;
    }

    if (!res.fatalError.empty())
    {
        std::cout << Diff::Paint("31", "[FAIL]") << " " << tc.name << timeStr
                  << "\n  " << res.fatalError << "\n";
        return TestResult::Fail;
    }

    bool allMatched = true;
    allMatched &= CheckSection(tc.hasAst, tc.expectedAst, res.outputs, TestCaseLoader::kExpectedAstMarker);
    allMatched &= CheckSection(tc.hasTypes, tc.expectedTypes, res.outputs, TestCaseLoader::kExpectedTypesMarker);
    allMatched &= CheckSection(tc.hasDisasm, tc.expectedDisasm, res.outputs, TestCaseLoader::kExpectedDisasmMarker);
    allMatched &= CheckSection(tc.hasVm, tc.expectedVm, res.outputs, TestCaseLoader::kExpectedVmMarker);

    if (tc.hasErrors || !res.errors.empty())
    {
        std::string expectedErrStr = TestCaseLoader::RenderExpected(tc.expectedErrors, res.errors);
        std::string actualErrStr = TestCaseLoader::RenderErrors(res.errors);
        if (expectedErrStr != actualErrStr)
            allMatched = false;
    }

    if (hasAnyExpectation && allMatched)
    {
        std::cout << Diff::Paint("32", "[PASS]") << " " << tc.name << timeStr << "\n";
        return TestResult::Pass;
    }

    if (opt.update)
    {
        std::string updated = TestCaseLoader::UpdatedContent(tc, res.outputs, res.errors);
        std::ofstream file(tc.path, std::ios::binary);
        if (!file)
        {
            std::cout << Diff::Paint("31", "[FAIL]") << " " << tc.name << timeStr
                      << "\n  Cannot write to " << tc.path << "\n";
            return TestResult::Fail;
        }
        file << updated;
        std::cout << Diff::Paint("33", "[UPDATED]") << " " << tc.name << timeStr << "\n";
        return TestResult::Updated;
    }

    std::cout << Diff::Paint("31", "[FAIL]") << " " << tc.name << timeStr << "\n";
    if (tc.hasErrors || !res.errors.empty())
    {
        std::string expectedErrStr = TestCaseLoader::RenderExpected(tc.expectedErrors, res.errors);
        std::string actualErrStr = TestCaseLoader::RenderErrors(res.errors);
        if (expectedErrStr != actualErrStr)
        {
            std::cout << "  Errors mismatch:\n";
            Diff::PrintDiff(expectedErrStr, actualErrStr);
        }
    }
    PrintSectionDiff(tc.hasAst, tc.expectedAst, res.outputs, TestCaseLoader::kExpectedAstMarker, "AST");
    PrintSectionDiff(tc.hasTypes, tc.expectedTypes, res.outputs, TestCaseLoader::kExpectedTypesMarker, "Types");
    PrintSectionDiff(tc.hasDisasm, tc.expectedDisasm, res.outputs, TestCaseLoader::kExpectedDisasmMarker, "Disasm");
    PrintSectionDiff(tc.hasVm, tc.expectedVm, res.outputs, TestCaseLoader::kExpectedVmMarker, "VM");

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

    float totalSuiteTimeMs = 0.0f;
    int passed = 0, updated = 0;
    std::vector<std::string> failed;

    {
        ZN_PROFILE_SCOPE(&totalSuiteTimeMs);
        for (const auto& tc : cases)
        {
            switch (RunCase(tc, opt))
            {
                case TestResult::Pass:    ++passed; break;
                case TestResult::Updated: ++updated; break;
                case TestResult::Fail:    failed.push_back(tc.name); break;
            }
        }
    }

    std::cout << "\n===============================\n"
              << "Total: " << cases.size() << " | Passed: " << passed
              << " | Failed: " << failed.size() << " | Updated: " << updated << "\n";
    for (const auto& name : failed) std::cout << "  failed: " << name << "\n";
    std::cout << "Total Execution Time: " << std::fixed << std::setprecision(2) << totalSuiteTimeMs << "ms\n";
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
