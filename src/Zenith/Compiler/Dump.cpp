#include <iostream>
#include <iomanip>
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

        for (size_t i = 0; i < m_Errors.size(); ++i)
        {
            const auto& err = m_Errors[i];
            
            uint32_t target_tok_idx = err.token; 
            size_t highlight_len = 1;

            if (err.node != ERROR_NODE && err.node < m_Nodes.size())
            {
                const auto& node = m_Nodes[err.node];
                target_tok_idx = node.start_token;
                
                const char* start_ptr = m_Tokens[node.start_token].literal.data();
                const char* end_ptr   = m_Tokens[node.end_token].literal.data() + m_Tokens[node.end_token].literal.size();
                highlight_len = std::max<size_t>(1, static_cast<size_t>(end_ptr - start_ptr));
            }
            else if (target_tok_idx < m_Tokens.size())
                highlight_len = std::max<size_t>(1, m_Tokens[target_tok_idx].literal.size());

            if (target_tok_idx >= m_Tokens.size()) continue;
            const Token& tok = m_Tokens[target_tok_idx];

            const char* line_start = tok.literal.data();
            size_t max_back = tok.column > 1 ? static_cast<size_t>(tok.column - 1) : 0;
            
            while (max_back > 0 && *(line_start - 1) != '\n' && *(line_start - 1) != '\r')
            {
                --line_start;
                --max_back;
            }

            const char* line_end = tok.literal.data();
            while (*line_end != '\0' && *line_end != '\n' && *line_end != '\r') ++line_end;

            std::string_view line_snippet(line_start, static_cast<size_t>(line_end - line_start));
            size_t indent_cols = static_cast<size_t>(tok.literal.data() - line_start);
            size_t max_line_remain = static_cast<size_t>(line_end - tok.literal.data());
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
        std::cout << "\n================ FLAT AST DUMP (" << m_Nodes.size() << " nodes) ================\n";
        
        std::cout << std::left 
                << std::setw(10) << "NODE" 
                << std::setw(24) << "TOKEN" 
                << std::setw(16) << "TYPE" 
                << "DETAILS\n";
        std::cout << std::string(80, '-') << "\n";

        auto fmt_idx = [](uint32_t idx) -> std::string {
            char buf[8];
            snprintf(buf, sizeof(buf), "%02u", idx);
            return std::string(buf);
        };

        for (size_t i = 1; i < m_Nodes.size(); ++i) 
        {
            const auto& node = m_Nodes[i];
            const auto& tok = m_Tokens[node.start_token];

            std::string node_str = "Node[" + fmt_idx(static_cast<uint32_t>(i)) + "]";
            std::cout << std::left << std::setw(10) << node_str;

            std::string tok_str = "Tok[" + fmt_idx(node.start_token) + "]: '" + std::string(tok.literal) + "'";
            std::cout << std::left << std::setw(24) << tok_str;

            switch (node.type) 
            {
                case NodeType::Integer:
                    std::cout << std::setw(16) << "[Integer]" 
                            << "Val: " << node.data.i64;
                    break;

                case NodeType::Decimal:
                    std::cout << std::setw(16) << "[Decimal]" 
                            << "Val: " << node.data.f64;
                    break;

                case NodeType::String:
                    std::cout << std::setw(16) << "[String]" 
                            << "Val: \"" << tok.literal << "\"";
                    break;

                case NodeType::Identifier:
                    std::cout << std::setw(16) << "[Identifier]" 
                            << "Name: " << tok.literal;
                    break;

                case NodeType::TypedIdentifier:
                    std::cout << std::setw(16) << "[TypedIdentifier]" 
                            << "Name: Node[" << fmt_idx(node.data.indicies.lhs) << "]"
                            << " | Type: Node[" << fmt_idx(node.data.indicies.rhs) << "]";
                    break;

                case NodeType::Label:
                    std::cout << std::setw(16) << "[Label]" 
                            << "Name: " << tok.literal;
                    break;

                case NodeType::UnaryExpr:
                    std::cout << std::setw(16) << "[UnaryExpr]" 
                            << "Child: Node[" << fmt_idx(node.data.indicies.lhs) << "]";
                    break;

                case NodeType::BinaryExpr:
                    std::cout << std::setw(16) << "[BinaryExpr]" 
                            << "LHS: Node[" << fmt_idx(node.data.indicies.lhs) << "]" 
                            << " | RHS: Node[" << fmt_idx(node.data.indicies.rhs) << "]";
                    break;

                case NodeType::FieldAccess:
                    std::cout << std::setw(16) << "[FieldAccess]" 
                            << "Obj: Node[" << fmt_idx(node.data.indicies.lhs) << "]" 
                            << " | Field: Node[" << fmt_idx(node.data.indicies.rhs) << "]";
                    break;

                case NodeType::IndexExpr:
                    std::cout << std::setw(16) << "[IndexExpr]" 
                            << "Arr: Node[" << fmt_idx(node.data.indicies.lhs) << "]" 
                            << " | Index: Node[" << fmt_idx(node.data.indicies.rhs) << "]";
                    break;

                case NodeType::BorrowExpr:
                    std::cout << std::setw(16) << "[BorrowExpr]" 
                            << "Target: Node[" << fmt_idx(node.data.indicies.lhs) << "]";
                    break;

                case NodeType::TypeAnnotation:
                    std::cout << std::setw(16) << "[TypeAnnotation]" 
                            << "IsBorrow: " << node.data.indicies.lhs 
                            << " | IsMut: " << node.data.indicies.rhs;
                    break;

                case NodeType::LetStmt:
                    std::cout << std::setw(16) << "[LetStmt]"
                            << "Ident: Node[" << fmt_idx(node.data.indicies.lhs) << "]"
                            << " | Init: Node[" << fmt_idx(node.data.indicies.rhs) << "]";
                    break;

                case NodeType::ReturnStmt:
                    std::cout << std::setw(16) << "[ReturnStmt]" 
                            << "Expr: " << (node.data.indicies.lhs != ERROR_NODE ? "Node[" + fmt_idx(node.data.indicies.lhs) + "]" : "None");
                    break;

                case NodeType::BreakStmt:
                {
                    std::string label_str = (node.data.indicies.lhs != ERROR_NODE) 
                                            ? "Node[" + fmt_idx(node.data.indicies.lhs) + "]" 
                                            : "None";
                                            
                    std::string expr_str  = (node.data.indicies.rhs != ERROR_NODE) 
                                            ? "Node[" + fmt_idx(node.data.indicies.rhs) + "]" 
                                            : "None";

                    std::cout << std::setw(16) << "[BreakStmt]" 
                            << "Label: " << label_str << " | Expr: " << expr_str;
                    break;
                }

                case NodeType::ContinueStmt:
                    std::cout << std::setw(16) << "[ContinueStmt]" 
                            << "Expr: " << (node.data.indicies.lhs != ERROR_NODE ? "Node[" + fmt_idx(node.data.indicies.lhs) + "]" : "None");
                    break;

                case NodeType::CallExpr:
                {
                    std::cout << std::setw(16) << "[CallExpr]";
                    uint32_t start = node.data.list.start;
                    uint32_t count = node.data.list.count;

                    if (count > 0 && start + count <= m_Extra.size()) 
                    {
                        uint32_t callee = m_Extra[start];
                        std::cout << "Callee: Node[" << fmt_idx(callee) << "] | Args: [";
                        
                        for (uint32_t c = 1; c < count; ++c) 
                        {
                            if (c > 1) std::cout << ", ";
                            std::cout << "Node[" << fmt_idx(m_Extra[start + c]) << "]";
                        }
                        std::cout << "]";
                    } 
                    else 
                    {
                        std::cout << "ExtraStart: " << start << " | Count: " << count;
                    }
                    break;
                }

                case NodeType::IfExpr:
                {
                    std::cout << std::setw(16) << "[IfExpr]";
                    uint32_t start = node.data.list.start;
                    uint32_t count = node.data.list.count;

                    if (count >= 3 && start + 2 < m_Extra.size())
                    {
                        uint32_t cond   = m_Extra[start];
                        uint32_t then_b = m_Extra[start + 1];
                        uint32_t else_b = m_Extra[start + 2];

                        std::cout << "Cond: Node[" << fmt_idx(cond) << "]"
                                << " | Then: Node[" << fmt_idx(then_b) << "]"
                                << " | Else: " << (else_b != ERROR_NODE ? "Node[" + fmt_idx(else_b) + "]" : "None");
                    }
                    else
                    {
                        std::cout << "ExtraStart: " << start << " | Count: " << count;
                    }
                    break;
                }

                case NodeType::ExprStmt:
                    std::cout << std::setw(16) << "[ExprStmt]" 
                            << "Expr: Node[" << fmt_idx(node.data.indicies.lhs) << "]";
                    break;

                case NodeType::LabeledExpr:
                    std::cout << std::setw(16) << "[LabeledExpr]" 
                            << "Label: Node[" << fmt_idx(node.data.indicies.lhs) 
                            << "] | Expr: Node[" << fmt_idx(node.data.indicies.rhs) << "]";
                    break;

                case NodeType::BlockExpr:
                {
                    std::cout << std::setw(16) << "[BlockExpr]";
                    uint32_t start = node.data.list.start;
                    uint32_t count = node.data.list.count;

                    if (count > 0 && start + count <= m_Extra.size())
                    {
                        std::cout << "Stmts: [";
                        for (uint32_t c = 0; c < count; ++c)
                        {
                            if (c > 0) std::cout << ", ";
                            std::cout << "Node[" << fmt_idx(m_Extra[start + c]) << "]";
                        }
                        std::cout << "]";
                    }
                    else
                    {
                        std::cout << "ExtraStart: " << start << " | Count: " << count;
                    }
                    break;
                }

                case NodeType::Program:
                {
                    std::cout << std::setw(16) << "[Program]";
                    uint32_t start = node.data.list.start;
                    uint32_t count = node.data.list.count;
                    if (count > 0 && start + count <= m_Extra.size())
                    {
                        std::cout << "Stmts Count: " << count;
                    }
                    break;
                }

                default:
                    std::cout << std::setw(16) << "[Unknown]";
                    break;
            }
            std::cout << "\n";
        }
        std::cout << std::string(80, '=') << "\n";
    }
}
