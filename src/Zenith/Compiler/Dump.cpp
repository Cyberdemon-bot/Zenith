#include <iostream>
#include <iomanip>
#include "Zenith/Compiler/Parser.h"

namespace Zenith {

    void Parser::dump_errors() const
    {
        for (const auto& err : m_Errors) std::cerr << err << '\n';
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
            const auto& tok = m_Tokens[node.token];

            std::string node_str = "Node[" + fmt_idx(static_cast<uint32_t>(i)) + "]";
            std::cout << std::left << std::setw(10) << node_str;

            std::string tok_str = "Tok[" + fmt_idx(node.token) + "]: '" + std::string(tok.literal) + "'";
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

                case NodeType::Label:
                    std::cout << std::setw(16) << "[Label]" 
                            << "Name: " << tok.literal;
                    break;

                case NodeType::UnaryExpr:
                    std::cout << std::setw(16) << "[UnaryExpr]" 
                            << "Child: " << fmt_idx(node.data.indicies.lhs);
                    break;

                case NodeType::BinaryExpr:
                    std::cout << std::setw(16) << "[BinaryExpr]" 
                            << "LHS: " << fmt_idx(node.data.indicies.lhs) 
                            << " | RHS: " << fmt_idx(node.data.indicies.rhs);
                    break;

                case NodeType::FieldAccess:
                    std::cout << std::setw(16) << "[FieldAccess]" 
                            << "Obj: " << fmt_idx(node.data.indicies.lhs) 
                            << " | Field: " << fmt_idx(node.data.indicies.rhs);
                    break;

                case NodeType::IndexExpr:
                    std::cout << std::setw(16) << "[IndexExpr]" 
                            << "Arr: " << fmt_idx(node.data.indicies.lhs) 
                            << " | Index: " << fmt_idx(node.data.indicies.rhs);
                    break;

                case NodeType::BorrowExpr:
                    std::cout << std::setw(16) << "[BorrowExpr]" 
                            << "Target: " << fmt_idx(node.data.indicies.lhs);
                    break;

                case NodeType::TypeAnnotation:
                    std::cout << std::setw(16) << "[TypeAnnotation]" 
                            << "IsBorrow: " << node.data.indicies.lhs 
                            << " | IsMut: " << node.data.indicies.rhs;
                    break;

                case NodeType::LetStmt:
                {
                    std::cout << std::setw(16) << "[LetStmt]";
                    uint32_t start = node.data.list.start;
                    uint32_t count = node.data.list.count;
                    if (count >= 3 && start + 2 < m_Extra.size())
                    {
                        std::cout << "Ident: Node[" << fmt_idx(m_Extra[start]) << "]"
                                  << " | Type: " << (m_Extra[start + 1] != ERROR_NODE ? "Node[" + fmt_idx(m_Extra[start + 1]) + "]" : "None")
                                  << " | Init: Node[" << fmt_idx(m_Extra[start + 2]) << "]";
                    }
                    break;
                }

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
                        std::cout << "StartIdx: " << start << " | Count: " << count;
                    }
                    break;
                }
                case NodeType::ExprStmt:
                    std::cout << "[ExprStmt]     Expr: Node[" << node.data.indicies.lhs << "]";
                    break;

                case NodeType::LabeledExpr:
                    std::cout << "[LabeledExpr]  Label: Node[" << node.data.indicies.lhs 
                            << "] | Expr: Node[" << node.data.indicies.rhs << "]";
                    break;

                case NodeType::BlockExpr:
                    std::cout << "[BlockExpr]    Stmts Start: " << node.data.list.start 
                            << " | Count: " << node.data.list.count;
                    break;

                default:
                    std::cout << std::setw(16) << "[Unknown]";
                    break;
            }
            std::cout << "\n";
        }
        std::cout << std::string(80, '=') << "\n";
    }
}
