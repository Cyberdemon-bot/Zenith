#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include "Zenith/Compiler/Parser.h"

namespace fs = std::filesystem;

namespace
{
    constexpr std::string_view kErrorsMarker = "// === EXPECTED ERRORS ===";
    constexpr std::string_view kAstMarker    = "// === EXPECTED AST ===";

    bool g_color = true;

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
        std::string source;
        std::string load_error;
        bool has_errors = false;
        bool has_ast = false;
        std::vector<ErrorInfo> expected_errors;
        std::string expected_ast;
    };

    struct Options
    {
        std::string dir = "tests/cases";
        std::string filter;
        bool update = false;
        bool dump = false;
    };

    enum class Result { Pass, Fail, Updated };

    std::string read_file(const fs::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file) throw std::runtime_error("cannot open " + path.string());
        return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }

    void write_file(const fs::path& path, const std::string& content)
    {
        std::ofstream file(path, std::ios::binary);
        if (!file) throw std::runtime_error("cannot write " + path.string());
        file << content;
    }

    std::string normalize_newlines(std::string s)
    {
        s.erase(std::remove(s.begin(), s.end(), '\r'), s.end());
        return s;
    }

    std::string trim(const std::string& s)
    {
        size_t b = s.find_first_not_of(" \t\n");
        if (b == std::string::npos) return "";
        return s.substr(b, s.find_last_not_of(" \t\n") - b + 1);
    }

    std::vector<std::string> split_lines(const std::string& s)
    {
        std::vector<std::string> lines;
        std::istringstream in(s);
        for (std::string line; std::getline(in, line);) lines.push_back(line);
        return lines;
    }

    std::string paint(const char* code, const std::string& text)
    {
        return g_color ? std::string("\033[") + code + "m" + text + "\033[0m" : text;
    }

    std::string format_error(const ErrorInfo& e)
    {
        std::string out = std::to_string(e.line) + ":" + std::to_string(e.col);
        if (!e.message.empty()) out += " " + e.message;
        return out;
    }

    std::string render_errors(const std::vector<ErrorInfo>& errors)
    {
        std::string out;
        for (const auto& e : errors) out += format_error(e) + "\n";
        return trim(out);
    }

    // An expected entry without a message matches any message at the same position.
    std::string render_expected(const std::vector<ErrorInfo>& expected, const std::vector<ErrorInfo>& actual)
    {
        std::string out;
        for (size_t i = 0; i < expected.size(); ++i)
        {
            ErrorInfo e = expected[i];
            bool same_pos = i < actual.size() && actual[i].line == e.line && actual[i].col == e.col;
            if (e.message.empty() && same_pos) e.message = actual[i].message;
            out += format_error(e) + "\n";
        }
        return trim(out);
    }

    bool parse_error_line(const std::string& s, ErrorInfo& out)
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
        out.message = trim(s.substr(i));
        return true;
    }

    // Reads errors from Parser::dump_errors: "[Error #N] <message>" followed by "--> Line L:C".
    std::vector<ErrorInfo> collect_errors(const Zenith::Parser& parser)
    {
        std::ostringstream out;
        parser.dump_errors(out);

        std::vector<ErrorInfo> errors;
        for (const auto& line : split_lines(normalize_newlines(out.str())))
        {
            if (line.rfind("[Error #", 0) == 0)
            {
                auto close = line.find("] ");
                errors.push_back({ 0, 0, close == std::string::npos ? "" : line.substr(close + 2) });
            }
            else if (!errors.empty() && errors.back().line == 0)
            {
                auto pos = line.find("--> Line ");
                unsigned l = 0, c = 0;
                if (pos != std::string::npos && std::sscanf(line.c_str() + pos + 9, "%u:%u", &l, &c) == 2)
                {
                    errors.back().line = l;
                    errors.back().col = c;
                }
            }
        }
        return errors;
    }

    TestCase load_case(const fs::path& path, const fs::path& root)
    {
        TestCase tc;
        tc.path = path.string();
        fs::path rel = fs::relative(path, root);
        rel.replace_extension();
        tc.name = rel.generic_string();

        std::string content = normalize_newlines(read_file(path));

        struct Marker { size_t pos; bool is_ast; };
        std::vector<Marker> markers;
        if (auto p = content.find(kErrorsMarker); p != std::string::npos) markers.push_back({ p, false });
        if (auto p = content.find(kAstMarker); p != std::string::npos) markers.push_back({ p, true });
        std::sort(markers.begin(), markers.end(), [](const Marker& a, const Marker& b) { return a.pos < b.pos; });

        tc.source = content.substr(0, markers.empty() ? content.size() : markers.front().pos);

        for (size_t i = 0; i < markers.size(); ++i)
        {
            size_t begin = markers[i].pos + (markers[i].is_ast ? kAstMarker.size() : kErrorsMarker.size());
            size_t end = i + 1 < markers.size() ? markers[i + 1].pos : content.size();
            std::string body = content.substr(begin, end - begin);

            if (markers[i].is_ast)
            {
                tc.has_ast = true;
                tc.expected_ast = trim(body);
                continue;
            }

            tc.has_errors = true;
            for (const auto& raw : split_lines(body))
            {
                std::string line = trim(raw);
                if (line.empty() || line.rfind("//", 0) == 0) continue;
                ErrorInfo e;
                if (!parse_error_line(line, e))
                {
                    tc.load_error = "invalid error expectation '" + line + "' (expected 'LINE:COL [message]')";
                    break;
                }
                tc.expected_errors.push_back(std::move(e));
            }
        }
        return tc;
    }

    std::vector<TestCase> load_cases(const fs::path& dir, const std::string& filter)
    {
        std::vector<TestCase> cases;
        for (const auto& entry : fs::recursive_directory_iterator(dir))
        {
            if (!entry.is_regular_file()) continue;
            auto ext = entry.path().extension();
            if (ext != ".test" && ext != ".zen") continue;

            TestCase tc = load_case(entry.path(), dir);
            if (!filter.empty() && tc.name.find(filter) == std::string::npos) continue;
            cases.push_back(std::move(tc));
        }
        std::sort(cases.begin(), cases.end(), [](const TestCase& a, const TestCase& b) { return a.name < b.name; });
        return cases;
    }

    void print_diff(const std::string& expected, const std::string& actual)
    {
        auto exp = split_lines(expected);
        auto act = split_lines(actual);
        size_t max_len = std::max(exp.size(), act.size());
        size_t first = 0;
        while (first < std::min(exp.size(), act.size()) && exp[first] == act[first]) ++first;

        size_t begin = first >= 3 ? first - 3 : 0;
        size_t end = std::min(first + 6, max_len);

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
            std::cout << paint("31", "  - " + no + " | " + e) << "\n";
            std::cout << paint("32", "  + " + no + " | " + a) << "\n";
        }
    }

    std::string updated_content(const TestCase& tc, const std::vector<ErrorInfo>& errors, const std::string& ast)
    {
        std::string out = tc.source;
        if (!out.empty() && out.back() != '\n') out += '\n';
        if (!errors.empty())
        {
            out.append(kErrorsMarker).append("\n").append(render_errors(errors)).append("\n\n");
        }
        if (tc.has_ast || errors.empty())
        {
            out.append(kAstMarker).append("\n").append(ast).append("\n");
        }
        return out;
    }

    void print_fail(const TestCase& tc, const std::string& reason)
    {
        std::cout << paint("31", "[FAIL]") << " " << tc.name << "\n  " << reason << "\n";
    }

    Result run_case(const TestCase& tc, const Options& opt)
    {
        if (!tc.load_error.empty())
        {
            print_fail(tc, tc.load_error);
            return Result::Fail;
        }
        if (trim(tc.source).empty())
        {
            print_fail(tc, "no source before the first expectation marker");
            return Result::Fail;
        }

        Zenith::Parser parser(tc.source);
        auto ast = parser.ParseProgram();

        std::ostringstream tree;
        parser.dump_tree_ast(tree, ast.root);
        std::string actual_ast = trim(normalize_newlines(tree.str()));
        std::vector<ErrorInfo> actual_errors = collect_errors(parser);
        std::string actual_errors_text = render_errors(actual_errors);

        if (opt.dump)
        {
            std::cout << "--- " << tc.name << " ---\n";
            if (!actual_errors.empty()) std::cout << actual_errors_text << "\n\n";
            std::cout << actual_ast << "\n\n";
        }

        bool has_expectations = tc.has_errors || tc.has_ast;
        bool errors_ok = render_expected(tc.expected_errors, actual_errors) == actual_errors_text;
        bool check_ast = tc.has_ast || !tc.has_errors;
        bool ast_ok = !check_ast || tc.expected_ast == actual_ast;

        if (has_expectations && errors_ok && ast_ok)
        {
            std::cout << paint("32", "[PASS]") << " " << tc.name << "\n";
            return Result::Pass;
        }

        if (opt.update)
        {
            write_file(tc.path, updated_content(tc, actual_errors, actual_ast));
            std::cout << paint("33", "[UPDATED]") << " " << tc.name << "\n";
            return Result::Updated;
        }

        if (!has_expectations)
        {
            print_fail(tc, "no expectations found; add an expected section or run with --update");
            return Result::Fail;
        }

        std::cout << paint("31", "[FAIL]") << " " << tc.name << "\n";
        if (!errors_ok)
        {
            std::cout << "  errors mismatch\n";
            print_diff(render_expected(tc.expected_errors, actual_errors), actual_errors_text);
        }
        if (!ast_ok)
        {
            std::cout << "  AST mismatch\n";
            print_diff(tc.expected_ast, actual_ast);
        }
        return Result::Fail;
    }

    void print_usage()
    {
        std::cout
            << "Usage: test [dir] [options]\n"
            << "  dir               directory with *.test files, searched recursively (default: tests/cases)\n"
            << "  -f, --filter STR  run only tests whose name contains STR\n"
            << "  -u, --update      rewrite expectations with the actual output\n"
            << "  -d, --dump        print the actual errors and AST of every test\n"
            << "      --no-color    disable colored output\n"
            << "  -h, --help        show this help\n";
    }
}

int main(int argc, char* argv[])
{
    Options opt;
    if (std::getenv("NO_COLOR")) g_color = false;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-u" || arg == "--update") opt.update = true;
        else if (arg == "-d" || arg == "--dump") opt.dump = true;
        else if (arg == "--no-color") g_color = false;
        else if (arg == "-h" || arg == "--help") { print_usage(); return 0; }
        else if ((arg == "-f" || arg == "--filter") && i + 1 < argc) opt.filter = argv[++i];
        else if (!arg.empty() && arg[0] == '-')
        {
            std::cerr << "Unknown or incomplete option: " << arg << "\n";
            print_usage();
            return 2;
        }
        else opt.dir = arg;
    }

    try
    {
        if (!fs::is_directory(opt.dir))
        {
            std::cerr << "Test directory not found: " << opt.dir << " (cwd: " << fs::current_path().string() << ")\n";
            return 2;
        }

        std::vector<TestCase> cases = load_cases(opt.dir, opt.filter);
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
            switch (run_case(tc, opt))
            {
                case Result::Pass:    ++passed; break;
                case Result::Updated: ++updated; break;
                case Result::Fail:    failed.push_back(tc.name); break;
            }
        }

        std::cout << "\n===============================\n"
                  << "Total: " << cases.size() << " | Passed: " << passed
                  << " | Failed: " << failed.size() << " | Updated: " << updated << "\n";
        for (const auto& name : failed) std::cout << "  failed: " << name << "\n";
        std::cout << "===============================\n";

        return failed.empty() ? 0 : 1;
    }
    catch (const std::exception& e)
    {
        std::cerr << "error: " << e.what() << "\n";
        return 2;
    }
}