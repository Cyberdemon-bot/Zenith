#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

enum class InputKind
{
    Source,
    Asm,
    Unknown
};

struct ErrorInfo
{
    uint32_t line = 0;
    uint32_t col = 0;
    std::string message;
};

struct TestCase
{
    std::string name;
    std::string path;
    InputKind kind = InputKind::Source;
    std::string inputSource;
    std::string loadError;

    bool hasAst = false;
    std::string expectedAst;

    bool hasTypes = false;
    std::string expectedTypes;

    bool hasErrors = false;
    std::vector<ErrorInfo> expectedErrors;

    bool hasVm = false;
    std::string expectedVm;

    bool hasDisasm = false;
    std::string expectedDisasm;
};

class TestCaseLoader
{
public:
    static constexpr std::string_view kInputSourceMarker    = "// === INPUT: SOURCE ===";
    static constexpr std::string_view kInputAsmMarker       = "// === INPUT: ASM ===";
    static constexpr std::string_view kInputBytecodeMarker  = "// === INPUT: BYTECODE ===";

    static constexpr std::string_view kExpectedErrorsMarker = "// === EXPECTED ERRORS ===";
    static constexpr std::string_view kExpectedAstMarker    = "// === EXPECTED AST ===";
    static constexpr std::string_view kExpectedTypesMarker  = "// === EXPECTED TYPES ===";
    static constexpr std::string_view kExpectedVmMarker     = "// === EXPECTED VM ===";
    static constexpr std::string_view kExpectedDisasmMarker = "// === EXPECTED DISASM ===";

    static TestCase LoadCase(const fs::path& path, const fs::path& root);
    static std::string FormatError(const ErrorInfo& e);
    static std::string RenderErrors(const std::vector<ErrorInfo>& errors);
    static std::string RenderExpected(const std::vector<ErrorInfo>& expected, const std::vector<ErrorInfo>& actual);
    static bool ParseErrorLine(const std::string& s, ErrorInfo& out);

    static std::string UpdatedContent(
        const TestCase& tc,
        const std::map<std::string, std::string>& outputs,
        const std::vector<ErrorInfo>& actualErrors
    );
};
