#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include "Zenith/Compiler/Parser.h"

namespace fs = std::filesystem;

static const std::string SEPARATOR = "// === EXPECTED AST ===";

struct TestCase
{
    std::string name;
    std::string filepath;
    std::string source;
    std::string expected_ast;
    bool        has_separator = false;
};

static std::string read_file(const fs::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) throw std::runtime_error("Cannot open: " + path.string());
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

static void write_file(const fs::path& path, const std::string& content)
{
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) throw std::runtime_error("Cannot write to: " + path.string());
    file << content;
}

static std::string normalize_newlines(std::string s)
{
    s.erase(std::remove(s.begin(), s.end(), '\r'), s.end());
    return s;
}

static std::string trim(const std::string& s)
{
    size_t b = s.find_first_not_of(" \n\t");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \n\t");
    return s.substr(b, e - b + 1);
}

// ------------------------------------------------------------------ loading

static std::vector<TestCase> load_test_cases(const fs::path& dir, const std::string& filter)
{
    std::vector<TestCase> cases;
    for (const auto& entry : fs::directory_iterator(dir))
    {
        auto ext = entry.path().extension();
        if (ext != ".test" && ext != ".zen") continue;

        TestCase tc;
        tc.name     = entry.path().stem().string();
        tc.filepath = entry.path().string();
        if (!filter.empty() && tc.name.find(filter) == std::string::npos) continue;

        std::string content = normalize_newlines(read_file(entry.path()));
        auto sep_pos = content.find(SEPARATOR);
        if (sep_pos == std::string::npos)
        {
            tc.source = content;
        }
        else
        {
            tc.has_separator = true;
            tc.source        = content.substr(0, sep_pos);
            tc.expected_ast  = trim(content.substr(sep_pos + SEPARATOR.size()));
        }
        cases.push_back(std::move(tc));
    }
    std::sort(cases.begin(), cases.end(),
              [](const TestCase& a, const TestCase& b) { return a.name < b.name; });
    return cases;
}

static std::vector<std::string> split_lines(const std::string& str)
{
    std::vector<std::string> lines;
    std::istringstream iss(str);
    std::string line;
    while (std::getline(iss, line)) lines.push_back(line);
    return lines;
}

static void print_smart_diff(const std::string& expected, const std::string& actual)
{
    auto exp_lines = split_lines(expected);
    auto act_lines = split_lines(actual);
    size_t min_len = std::min(exp_lines.size(), act_lines.size());
    size_t max_len = std::max(exp_lines.size(), act_lines.size());

    size_t first_diff = min_len; 
    for (size_t i = 0; i < min_len; ++i)
    {
        if (exp_lines[i] != act_lines[i]) { first_diff = i; break; }
    }

    if (first_diff == min_len && exp_lines.size() == act_lines.size())
    {
        std::cout << "  (Hai chuỗi giống nhau theo từng dòng, khác ở khoảng trắng cuối dòng/tab)\n";
        return;
    }

    size_t start = (first_diff >= 3) ? first_diff - 3 : 0;
    size_t end   = std::min(first_diff + 6, max_len); // dùng max_len, không phải min_len

    std::cout << "  --- Smart Diff (dòng " << (start + 1) << " đến " << end << ") ---\n";
    for (size_t i = start; i < end; ++i)
    {
        bool has_e = i < exp_lines.size();
        bool has_a = i < act_lines.size();
        const std::string e = has_e ? exp_lines[i] : "<EOF>";
        const std::string a = has_a ? act_lines[i] : "<EOF>";

        if (e != a)
        {
            std::cout << "\033[31m- " << std::setw(4) << (i + 1) << " | " << e << "\033[0m\n";
            std::cout << "\033[32m+ " << std::setw(4) << (i + 1) << " | " << a << "\033[0m\n";
        }
        else
        {
            std::cout << "  " << std::setw(4) << (i + 1) << " | " << e << "\n";
        }
    }
    if (exp_lines.size() != act_lines.size())
    {
        std::cout << "  ... (Tổng số dòng: Expected " << exp_lines.size()
                  << ", Actual " << act_lines.size() << ")\n";
    }
}

static void print_usage()
{
    std::cout << "Usage: test [dir] [--update|-u] [--filter|-f <substr>] [--dump|-d]\n"
              << "  dir          thư mục chứa *.test (mặc định: tests/cases)\n"
              << "  --update     ghi đè phần expected bằng output hiện tại (chỉ khi parse không lỗi)\n"
              << "  --filter     chỉ chạy test có tên chứa <substr>\n"
              << "  --dump       in AST thực tế của mỗi test (để debug)\n";
}

int main(int argc, char* argv[])
{
    bool update_mode = false;
    bool dump_mode   = false;
    std::string test_dir = "tests/cases";
    std::string filter;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--update" || arg == "-u")      update_mode = true;
        else if (arg == "--dump" || arg == "-d")   dump_mode = true;
        else if ((arg == "--filter" || arg == "-f") && i + 1 < argc) filter = argv[++i];
        else if (arg == "--help" || arg == "-h")   { print_usage(); return 0; }
        else                                       test_dir = arg;
    }

    if (!fs::exists(test_dir))
    {
        std::cerr << "Test directory not found: " << test_dir
                  << "  (cwd = " << fs::current_path().string() << ")\n";
        return 1;
    }

    auto cases = load_test_cases(test_dir, filter);
    int passed = 0, failed = 0;

    std::cout << "Running " << cases.size() << " test cases..."
              << (update_mode ? " [UPDATE MODE]" : "") << "\n\n";

    for (const auto& tc : cases)
    {
        if (trim(tc.source).empty())
        {
            std::cout << "\033[31m[FAIL]\033[0m " << tc.name
                      << "  -> file KHÔNG có source phía trên '" << SEPARATOR << "'\n";
            failed++;
            continue;
        }

        Zenith::Parser parser(tc.source);
        uint32_t root_idx = parser.parse_program();

        std::stringstream ss;
        parser.dump_tree_ast(ss, root_idx);
        std::string actual_ast = trim(normalize_newlines(ss.str()));

        if (dump_mode) std::cout << actual_ast << "\n";

        if (parser.has_errors())
        {
            std::cout << "\033[31m[FAIL]\033[0m " << tc.name << "  -> parser báo "
                      << parser.get_errors_count() << " lỗi:\n";
            parser.dump_errors(std::cout);
            failed++;
            continue;
        }

        if (actual_ast == tc.expected_ast)
        {
            std::cout << "\033[32m[PASS]\033[0m " << tc.name << "\n";
            passed++;
        }
        else if (update_mode)
        {
            write_file(tc.filepath, tc.source + SEPARATOR + "\n" + actual_ast + "\n");
            std::cout << "\033[33m[UPDATED]\033[0m " << tc.name << "\n";
            passed++;
        }
        else
        {
            std::cout << "\033[31m[FAIL]\033[0m " << tc.name << "\n";
            print_smart_diff(tc.expected_ast, actual_ast);
            failed++;
        }
    }

    std::cout << "\n===============================\n";
    std::cout << "Total: " << cases.size()
              << " | Passed: " << passed
              << " | Failed: " << failed << "\n";
    std::cout << "===============================\n";

    return failed > 0 ? 1 : 0;
}