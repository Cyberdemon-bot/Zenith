#pragma once

#include "TestCase.h"

#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct Options
{
    std::string dir = "test/cases";
    std::string filter;
    bool update = false;
    bool dump = false;
};

enum class TestResult
{
    Pass,
    Fail,
    Updated
};

class Tester
{
public:
    static int Run(const Options& opt);
    static TestResult RunCase(const TestCase& tc, const Options& opt);
    static std::vector<TestCase> LoadCases(const fs::path& dir, const std::string& filter);
    static void PrintUsage();
};
