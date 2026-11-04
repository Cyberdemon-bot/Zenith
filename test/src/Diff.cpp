#include "Diff.h"

#include <algorithm>
#include <iostream>
#include <sstream>

bool Diff::s_Color = true;

std::string Diff::Paint(const char* code, const std::string& text)
{
    return s_Color ? std::string("\033[") + code + "m" + text + "\033[0m" : text;
}

std::string Diff::NormalizeNewlines(std::string s)
{
    s.erase(std::remove(s.begin(), s.end(), '\r'), s.end());
    return s;
}

std::string Diff::Trim(const std::string& s)
{
    size_t b = s.find_first_not_of(" \t\n");
    if (b == std::string::npos) return "";
    return s.substr(b, s.find_last_not_of(" \t\n") - b + 1);
}

std::vector<std::string> Diff::SplitLines(const std::string& s)
{
    std::vector<std::string> lines;
    std::istringstream in(s);
    for (std::string line; std::getline(in, line);) lines.push_back(line);
    return lines;
}

void Diff::PrintDiff(const std::string& expected, const std::string& actual)
{
    auto exp = SplitLines(expected);
    auto act = SplitLines(actual);
    size_t maxLen = std::max(exp.size(), act.size());
    size_t first = 0;
    while (first < std::min(exp.size(), act.size()) && exp[first] == act[first]) ++first;

    size_t begin = first >= 3 ? first - 3 : 0;
    size_t end = std::min(first + 6, maxLen);

    std::cout << "  diff, lines " << begin + 1 << "-" << end
              << " (expected " << exp.size() << " lines, actual " << act.size() << ")\n";
    for (size_t i = begin; i < end; ++i)
    {
        std::string e = i < exp.size() ? exp[i] : "<end>";
        std::string a = i < act.size() ? act[i] : "<end>";
        std::string no = std::to_string(i + 1);
        no.insert(0, no.size() < 4 ? 4 - no.size() : 0, ' ');

        if (e == a)
        {
            std::cout << "    " << no << " | " << e << "\n";
            continue;
        }
        std::cout << Paint("31", "  - " + no + " | " + e) << "\n";
        std::cout << Paint("32", "  + " + no + " | " + a) << "\n";
    }
}
