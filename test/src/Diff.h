#pragma once

#include <string>
#include <string_view>
#include <vector>

class Diff
{
public:
    static bool s_Color;

    static std::string Paint(const char* code, const std::string& text);
    static std::string NormalizeNewlines(std::string s);
    static std::string Trim(const std::string& s);
    static std::vector<std::string> SplitLines(const std::string& s);
    static void PrintDiff(const std::string& expected, const std::string& actual);
};
