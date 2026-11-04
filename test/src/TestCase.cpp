#include "TestCase.h"
#include "Diff.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <sstream>

namespace {

    std::string ReadFile(const fs::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file) throw std::runtime_error("Cannot open " + path.string());
        return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }

} // namespace

std::string TestCaseLoader::FormatError(const ErrorInfo& e)
{
    std::string out = std::to_string(e.line) + ":" + std::to_string(e.col);
    if (!e.message.empty()) out += " " + e.message;
    return out;
}

std::string TestCaseLoader::RenderErrors(const std::vector<ErrorInfo>& errors)
{
    std::string out;
    for (const auto& e : errors) out += FormatError(e) + "\n";
    return Diff::Trim(out);
}

std::string TestCaseLoader::RenderExpected(const std::vector<ErrorInfo>& expected, const std::vector<ErrorInfo>& actual)
{
    std::string out;
    for (size_t i = 0; i < expected.size(); ++i)
    {
        ErrorInfo e = expected[i];
        bool samePos = i < actual.size() && actual[i].line == e.line && actual[i].col == e.col;
        if (e.message.empty() && samePos) e.message = actual[i].message;
        out += FormatError(e) + "\n";
    }
    return Diff::Trim(out);
}

bool TestCaseLoader::ParseErrorLine(const std::string& s, ErrorInfo& out)
{
    size_t i = 0;
    auto number = [&](uint32_t& v)
    {
        size_t begin = i;
        v = 0;
        while (i < s.size() && isdigit(static_cast<unsigned char>(s[i]))) v = v * 10 + (s[i++] - '0');
        return i > begin;
    };
    if (!number(out.line) || i >= s.size() || s[i++] != ':' || !number(out.col)) return false;
    out.message = Diff::Trim(s.substr(i));
    return true;
}

TestCase TestCaseLoader::LoadCase(const fs::path& path, const fs::path& root)
{
    TestCase tc;
    tc.path = path.string();
    fs::path rel = fs::relative(path, root);
    rel.replace_extension();
    tc.name = rel.generic_string();

    std::string content = Diff::NormalizeNewlines(ReadFile(path));

    // Determine input kind from header
    size_t sourceOffset = 0;
    std::string trimmed = Diff::Trim(content);
    if (trimmed.starts_with(kInputSourceMarker))
    {
        tc.kind = InputKind::Source;
        size_t pos = content.find(kInputSourceMarker);
        sourceOffset = pos + kInputSourceMarker.size();
    }
    else if (trimmed.starts_with(kInputAsmMarker))
    {
        tc.kind = InputKind::Asm;
        size_t pos = content.find(kInputAsmMarker);
        sourceOffset = pos + kInputAsmMarker.size();
    }
    else if (trimmed.starts_with(kInputBytecodeMarker))
    {
        tc.kind = InputKind::Asm;
        size_t pos = content.find(kInputBytecodeMarker);
        sourceOffset = pos + kInputBytecodeMarker.size();
    }
    else
    {
        // Fallback default for existing files without header marker
        tc.kind = InputKind::Source;
        sourceOffset = 0;
    }

    struct MarkerPos
    {
        size_t pos;
        std::string_view tag;
    };

    std::vector<MarkerPos> markers;
    auto checkMarker = [&](std::string_view m)
    {
        if (auto p = content.find(m, sourceOffset); p != std::string::npos)
            markers.push_back({ p, m });
    };

    checkMarker(kExpectedErrorsMarker);
    checkMarker(kExpectedAstMarker);
    checkMarker(kExpectedVmMarker);
    checkMarker(kExpectedDisasmMarker);

    std::sort(markers.begin(), markers.end(), [](const MarkerPos& a, const MarkerPos& b)
    {
        return a.pos < b.pos;
    });

    size_t inputEnd = markers.empty() ? content.size() : markers.front().pos;
    tc.inputSource = content.substr(sourceOffset, inputEnd - sourceOffset);
    if (!tc.inputSource.empty() && tc.inputSource.front() == '\n')
        tc.inputSource.erase(0, 1);

    for (size_t i = 0; i < markers.size(); ++i)
    {
        size_t begin = markers[i].pos + markers[i].tag.size();
        size_t end = (i + 1 < markers.size()) ? markers[i + 1].pos : content.size();
        std::string body = content.substr(begin, end - begin);

        if (markers[i].tag == kExpectedAstMarker)
        {
            tc.hasAst = true;
            tc.expectedAst = Diff::Trim(body);
        }
        else if (markers[i].tag == kExpectedVmMarker)
        {
            tc.hasVm = true;
            tc.expectedVm = Diff::Trim(body);
        }
        else if (markers[i].tag == kExpectedDisasmMarker)
        {
            tc.hasDisasm = true;
            tc.expectedDisasm = Diff::Trim(body);
        }
        else if (markers[i].tag == kExpectedErrorsMarker)
        {
            tc.hasErrors = true;
            for (const auto& raw : Diff::SplitLines(body))
            {
                std::string line = Diff::Trim(raw);
                if (line.empty() || line.rfind("//", 0) == 0) continue;
                ErrorInfo e;
                if (!ParseErrorLine(line, e))
                {
                    tc.loadError = "Invalid error expectation '" + line + "' (expected 'LINE:COL [message]')";
                    break;
                }
                tc.expectedErrors.push_back(std::move(e));
            }
        }
    }

    return tc;
}

std::string TestCaseLoader::UpdatedContent(
    const TestCase& tc,
    const std::map<std::string, std::string>& outputs,
    const std::vector<ErrorInfo>& actualErrors)
{
    std::string out;
    if (tc.kind == InputKind::Asm)
        out += std::string(kInputAsmMarker) + "\n";
    else
        out += std::string(kInputSourceMarker) + "\n";

    out += tc.inputSource;
    if (!out.empty() && out.back() != '\n') out += '\n';

    if (!actualErrors.empty())
    {
        out.append(kExpectedErrorsMarker).append("\n")
           .append(RenderErrors(actualErrors)).append("\n\n");
    }

    if (auto it = outputs.find(std::string(kExpectedAstMarker)); it != outputs.end())
    {
        out.append(kExpectedAstMarker).append("\n").append(it->second).append("\n\n");
    }
    if (auto it = outputs.find(std::string(kExpectedVmMarker)); it != outputs.end())
    {
        out.append(kExpectedVmMarker).append("\n").append(it->second).append("\n\n");
    }
    if (auto it = outputs.find(std::string(kExpectedDisasmMarker)); it != outputs.end())
    {
        out.append(kExpectedDisasmMarker).append("\n").append(it->second).append("\n\n");
    }

    while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
    out += '\n';
    return out;
}
