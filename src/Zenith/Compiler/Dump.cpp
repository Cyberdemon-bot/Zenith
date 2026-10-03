#include <iostream>
#include <iomanip>
#include <format>
#include "Zenith/Compiler/Parser.h"

namespace Zenith {

    void Parser::dump_errors() const
    {
        if (m_Errors.empty())
        {
            std::cerr << "[Parser] No errors found.\n";
            return;
        }

        std::cerr << std::format("=== Found {} Parse Error(s) ===\n\n", m_Errors.size());

        const char* src_begin = m_Source.data();
        const char* src_end   = src_begin + m_Source.size();

        for (size_t i = 0; i < m_Errors.size(); ++i)
        {
            const auto& err = m_Errors[i];

            uint32_t target_tok_idx = err.token;
            size_t highlight_len = 1;

            if (err.node != ERROR_NODE && err.node < m_Nodes.size())
            {
                const auto& node = m_Nodes[err.node];
                if (node.start_token >= m_Tokens.size() ||
                    node.end_token   >= m_Tokens.size())
                    continue;

                target_tok_idx = node.start_token;

                const char* start_ptr = m_Tokens[node.start_token].literal.data();
                const char* end_ptr   = m_Tokens[node.end_token].literal.data()
                                      + m_Tokens[node.end_token].literal.size();
                highlight_len = std::max<size_t>(1, static_cast<size_t>(end_ptr - start_ptr));
            }
            else if (target_tok_idx < m_Tokens.size())
            {
                highlight_len = std::max<size_t>(1, m_Tokens[target_tok_idx].literal.size());
            }

            if (target_tok_idx >= m_Tokens.size()) continue;
            const Token& tok = m_Tokens[target_tok_idx];
            if (tok.literal.empty()) continue;

            const char* tok_begin = tok.literal.data();

            // Quét lùi về đầu dòng, có chặn biên bằng src_begin.
            const char* line_start = tok_begin;
            while (line_start > src_begin &&
                   line_start[-1] != '\n' && line_start[-1] != '\r')
                --line_start;

            // Quét tới cuối dòng, có chặn biên bằng src_end.
            const char* line_end = tok_begin;
            while (line_end < src_end &&
                   *line_end != '\n' && *line_end != '\r')
                ++line_end;

            std::string_view line_snippet(line_start,
                                          static_cast<size_t>(line_end - line_start));
            size_t indent_cols     = static_cast<size_t>(tok_begin - line_start);
            size_t max_line_remain = static_cast<size_t>(line_end - tok_begin);
            highlight_len = std::max<size_t>(1, std::min(highlight_len, max_line_remain));

            std::cerr << std::format("[Error #{}] {}\n", i + 1, err.message);
            std::cerr << std::format("  --> Line {}:{}\n", tok.line, tok.column);
            std::cerr << "   |\n";
            std::cerr << std::format("{:>3} | {}\n", tok.line, line_snippet);
            std::cerr << std::format("   | {}{}\n\n",
                                     std::string(indent_cols, ' '),
                                     std::string(highlight_len, '^'));
        }
    }


    void Parser::dump_ast() const
    {
        std::cout << "\n================ FLAT AST DUMP ("
                  << m_Nodes.size() << " nodes) ================\n";

        std::cout << std::left
                  << std::setw(10) << "NODE"
                  << std::setw(24) << "TOKEN"
                  << std::setw(20) << "TYPE"
                  << "DETAILS\n";
        std::cout << std::string(90, '-') << "\n";

        auto fmt_idx = [](uint32_t idx) -> std::string {
            char buf[16];
            snprintf(buf, sizeof(buf), "%02u", idx);
            return std::string(buf);
        };

        auto node_ref = [&](uint32_t idx) -> std::string {
            return (idx == ERROR_NODE) ? std::string("None")
                                       : "Node[" + fmt_idx(idx) + "]";
        };

        auto dump_list = [&](uint32_t start, uint32_t count,
                             std::string_view label) {
            std::cout << label << ": [";
            for (uint32_t c = 0; c < count; ++c)
            {
                if (c > 0) std::cout << ", ";
                uint32_t e = m_Extra[start + c];
                std::cout << (e == ERROR_NODE ? "None" : "Node[" + fmt_idx(e) + "]");
            }
            std::cout << "]";
        };

        for (size_t i = 1; i < m_Nodes.size(); ++i)
        {
            const auto& node = m_Nodes[i];
            const auto& tok  = m_Tokens[node.start_token];

            std::string node_str = "Node[" + fmt_idx(static_cast<uint32_t>(i)) + "]";
            std::cout << std::left << std::setw(10) << node_str;

            std::string tok_str = "Tok[" + fmt_idx(node.start_token) + "]: '"
                                + std::string(tok.literal) + "'";
            std::cout << std::left << std::setw(24) << tok_str;

            switch (node.type)
            {
                case NodeType::Integer:
                    std::cout << std::setw(20) << "[Integer]"
                              << "Val: " << node.data.i64;
                    break;

                case NodeType::Decimal:
                    std::cout << std::setw(20) << "[Decimal]"
                              << "Val: " << node.data.f64;
                    break;

                case NodeType::String:
                    std::cout << std::setw(20) << "[String]"
                              << "Val: \"" << tok.literal << "\"";
                    break;

                case NodeType::Identifier:
                    std::cout << std::setw(20) << "[Identifier]"
                              << "Name: " << tok.literal;
                    break;

                case NodeType::TypedIdentifier:
                    std::cout << std::setw(20) << "[TypedIdentifier]"
                              << "Name: " << node_ref(node.data.indicies.lhs)
                              << " | Type: " << node_ref(node.data.indicies.rhs);
                    break;

                case NodeType::Label:
                    std::cout << std::setw(20) << "[Label]"
                              << "Name: " << tok.literal;
                    break;

                case NodeType::UnaryExpr:
                    std::cout << std::setw(20) << "[UnaryExpr]"
                              << "Child: " << node_ref(node.data.indicies.lhs);
                    break;

                case NodeType::BinaryExpr:
                    std::cout << std::setw(20) << "[BinaryExpr]"
                              << "LHS: " << node_ref(node.data.indicies.lhs)
                              << " | RHS: " << node_ref(node.data.indicies.rhs);
                    break;

                case NodeType::FieldAccess:
                    std::cout << std::setw(20) << "[FieldAccess]"
                              << "Obj: " << node_ref(node.data.indicies.lhs)
                              << " | Field: " << node_ref(node.data.indicies.rhs);
                    break;

                case NodeType::IndexExpr:
                    std::cout << std::setw(20) << "[IndexExpr]"
                              << "Arr: " << node_ref(node.data.indicies.lhs)
                              << " | Index: " << node_ref(node.data.indicies.rhs);
                    break;

                case NodeType::BorrowExpr:
                    std::cout << std::setw(20) << "[BorrowExpr]"
                              << "Target: " << node_ref(node.data.indicies.lhs);
                    break;

                case NodeType::TypeAnnotation:
                {
                    std::cout << std::setw(20) << "[TypeAnnotation]";
                    bool is_borrow = node.data.indicies.lhs != 0;
                    bool is_mut    = node.data.indicies.rhs != 0;
                    // Type ident nằm ở token end_token (xem parse_type_annotation).
                    std::string_view type_name =
                        (node.end_token < m_Tokens.size())
                            ? m_Tokens[node.end_token].literal
                            : std::string_view{};
                    std::cout << "Type: "
                              << (is_borrow ? "&" : "")
                              << (is_mut    ? "mut " : "")
                              << type_name;
                    break;
                }

                case NodeType::LetStmt:
                    std::cout << std::setw(20) << "[LetStmt]"
                              << "Ident: " << node_ref(node.data.indicies.lhs)
                              << " | Init: " << node_ref(node.data.indicies.rhs);
                    break;

                case NodeType::AssignStmt:
                    std::cout << std::setw(20) << "[AssignStmt]"
                              << "Target: " << node_ref(node.data.indicies.lhs)
                              << " | Value: "  << node_ref(node.data.indicies.rhs);
                    break;

                case NodeType::ReturnStmt:
                    std::cout << std::setw(20) << "[ReturnStmt]"
                              << "Expr: " << node_ref(node.data.indicies.lhs);
                    break;

                case NodeType::BreakStmt:
                    std::cout << std::setw(20) << "[BreakStmt]"
                              << "Label: " << node_ref(node.data.indicies.lhs)
                              << " | Expr: " << node_ref(node.data.indicies.rhs);
                    break;

                case NodeType::ContinueStmt:
                    std::cout << std::setw(20) << "[ContinueStmt]";
                    break;

                case NodeType::CallExpr:
                {
                    std::cout << std::setw(20) << "[CallExpr]";
                    uint32_t callee = node.data.indicies.lhs;
                    uint32_t args   = node.data.indicies.rhs;

                    std::cout << "Callee: " << node_ref(callee);

                    if (args != ERROR_NODE && args < m_Nodes.size())
                    {
                        const auto& arg_list = m_Nodes[args];
                        uint32_t s = arg_list.data.list.start;
                        uint32_t c = arg_list.data.list.count;
                        std::cout << " | ";
                        dump_list(s, c, "Args");
                    }
                    break;
                }

                case NodeType::ArgList:
                {
                    std::cout << std::setw(20) << "[ArgList]";
                    uint32_t s = node.data.list.start;
                    uint32_t c = node.data.list.count;
                    dump_list(s, c, "Args");
                    break;
                }

                case NodeType::IfExpr:
                {
                    std::cout << std::setw(20) << "[IfExpr]";
                    uint32_t s = node.data.list.start;
                    uint32_t c = node.data.list.count;
                    if (c >= 3 && s + 2 < m_Extra.size())
                    {
                        uint32_t cond   = m_Extra[s];
                        uint32_t then_b = m_Extra[s + 1];
                        uint32_t else_b = m_Extra[s + 2];
                        std::cout << "Cond: " << node_ref(cond)
                                  << " | Then: " << node_ref(then_b)
                                  << " | Else: " << node_ref(else_b);
                    }
                    else
                    {
                        std::cout << "ExtraStart: " << s << " | Count: " << c;
                    }
                    break;
                }

                case NodeType::ExprStmt:
                    std::cout << std::setw(20) << "[ExprStmt]"
                              << "Expr: " << node_ref(node.data.indicies.lhs);
                    break;

                case NodeType::LabeledExpr:
                    std::cout << std::setw(20) << "[LabeledExpr]"
                              << "Label: " << node_ref(node.data.indicies.lhs)
                              << " | Expr: " << node_ref(node.data.indicies.rhs);
                    break;

                case NodeType::BlockExpr:
                {
                    std::cout << std::setw(20) << "[BlockExpr]";
                    uint32_t s = node.data.list.start;
                    uint32_t c = node.data.list.count;
                    dump_list(s, c, "Stmts");
                    break;
                }

                case NodeType::Program:
                {
                    std::cout << std::setw(20) << "[Program]";
                    uint32_t s = node.data.list.start;
                    uint32_t c = node.data.list.count;
                    std::cout << "Stmts: " << c;
                    break;
                }

                default:
                    std::cout << std::setw(20)
                              << ("[Unknown:" +
                                  std::to_string(static_cast<uint32_t>(node.type)) +
                                  "]");
                    break;
            }
            std::cout << "\n";
        }
        std::cout << std::string(90, '=') << "\n";
    }
}