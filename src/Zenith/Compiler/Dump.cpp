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
            if (target_tok_idx >= m_Tokens.size())
                target_tok_idx = static_cast<uint32_t>(m_Tokens.size()) - 1;

            const Token& tok = m_Tokens[target_tok_idx];

            std::cerr << std::format("[Error #{}] {}\n", i + 1, err.message);
            std::cerr << std::format("  --> Line {}:{}\n", tok.line, tok.column);

            if (tok.literal.empty())
            {
                std::cerr << "\n";
                continue;
            }

            size_t highlight_len = std::max<size_t>(1, tok.literal.size());

            if (err.scope == ErrorScope::NodeStart &&
                err.node != ERROR_NODE && err.node < m_Nodes.size())
            {
                const auto& node = m_Nodes[err.node];
                if (node.start_token < m_Tokens.size() &&
                    node.end_token   < m_Tokens.size())
                {
                    const char* start_ptr = m_Tokens[node.start_token].literal.data();
                    const char* end_ptr   = m_Tokens[node.end_token].literal.data()
                                          + m_Tokens[node.end_token].literal.size();
                    highlight_len = std::max<size_t>(1,
                        static_cast<size_t>(end_ptr - start_ptr));
                }
            }

            const char* tok_begin = tok.literal.data();

            const char* line_start = tok_begin;
            while (line_start > src_begin &&
                   line_start[-1] != '\n' && line_start[-1] != '\r')
                --line_start;

            const char* line_end = tok_begin;
            while (line_end < src_end &&
                   *line_end != '\n' && *line_end != '\r')
                ++line_end;

            std::string_view line_snippet(line_start,
                                          static_cast<size_t>(line_end - line_start));
            size_t indent_cols     = static_cast<size_t>(tok_begin - line_start);
            size_t max_line_remain = static_cast<size_t>(line_end - tok_begin);
            highlight_len = std::max<size_t>(1,
                std::min(highlight_len, max_line_remain));

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

        auto tok_ref = [&](uint32_t idx) -> std::string {
            if (idx == ERROR_NODE || idx >= m_Tokens.size()) return std::string("None");
            return "Tok[" + fmt_idx(idx) + "]";
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

                case NodeType::Bool:
                    std::cout << std::setw(20) << "[Bool]"
                              << "Val: " << (node.data.i64 ? "true" : "false");
                    break;

                case NodeType::Null:
                    std::cout << std::setw(20) << "[Null]";
                    break;

                case NodeType::Identifier:
                    std::cout << std::setw(20) << "[Identifier]"
                              << "Name: " << tok.literal;
                    break;

                case NodeType::TypedIdentifier:
                    std::cout << std::setw(20) << "[TypedIdentifier]"
                              << "Name: " << node_ref(node.data.typed_ident.ident)
                              << " | Type: " << node_ref(node.data.typed_ident.type)
                              << " | Mut: "  << (node.data.typed_ident.is_mut ? "yes" : "no");
                    break;

                case NodeType::Label:
                    std::cout << std::setw(20) << "[Label]"
                              << "Name: " << tok.literal;
                    break;

                case NodeType::UnaryExpr:
                    std::cout << std::setw(20) << "[UnaryExpr]"
                              << "Op: "    << tok_ref(node.data.unary.op)
                              << " | Child: " << node_ref(node.data.unary.operand);
                    break;

                case NodeType::BinaryExpr:
                {
                    uint32_t op_tok = node.data.binary.op;
                    std::string_view op_str =
                        (op_tok < m_Tokens.size()) ? m_Tokens[op_tok].literal
                                                   : std::string_view{};
                    std::cout << std::setw(20) << "[BinaryExpr]"
                              << "LHS: " << node_ref(node.data.binary.lhs)
                              << " | Op: '" << op_str << "'"
                              << " | RHS: " << node_ref(node.data.binary.rhs);
                    break;
                }

                case NodeType::FieldAccess:
                    std::cout << std::setw(20) << "[FieldAccess]"
                              << "Obj: "   << node_ref(node.data.access.target)
                              << " | Field: " << node_ref(node.data.access.member);
                    break;

                case NodeType::IndexExpr:
                    std::cout << std::setw(20) << "[IndexExpr]"
                              << "Arr: "   << node_ref(node.data.access.target)
                              << " | Index: " << node_ref(node.data.access.member);
                    break;

                case NodeType::BorrowExpr:
                    std::cout << std::setw(20) << "[BorrowExpr]"
                              << "Mut: "     << (node.data.borrow.is_mut ? "yes" : "no")
                              << " | Target: " << node_ref(node.data.borrow.operand);
                    break;

                case NodeType::TypeAnnotation:
                {
                    uint8_t flags = node.data.type_annot.flags;
                    uint32_t ident_tok = node.data.type_annot.ident;

                    std::string_view type_name =
                        (ident_tok < m_Tokens.size()) ? m_Tokens[ident_tok].literal
                                                    : std::string_view{};

                    std::cout << std::setw(20) << "[TypeAnnotation]"
                            << "Type: "
                            << ((flags & TYPE_FLAG_BORROW) ? "&" : "")
                            << ((flags & TYPE_FLAG_MUT)    ? "mut " : "")
                            << type_name;
                    break;
                }

                case NodeType::LetStmt:
                    std::cout << std::setw(20) << "[LetStmt]"
                              << "Target: " << node_ref(node.data.let.target)
                              << " | Init: " << node_ref(node.data.let.value);
                    break;

                case NodeType::AssignStmt:
                {
                    uint32_t op_tok = node.data.binary.op;
                    std::string_view op_str =
                        (op_tok < m_Tokens.size()) ? m_Tokens[op_tok].literal
                                                   : std::string_view{};
                    std::cout << std::setw(20) << "[AssignStmt]"
                              << "Target: " << node_ref(node.data.binary.lhs)
                              << " | Op: '" << op_str << "'"
                              << " | Value: " << node_ref(node.data.binary.rhs);
                    break;
                }

                case NodeType::ReturnStmt:
                    std::cout << std::setw(20) << "[ReturnStmt]"
                              << "Expr: " << node_ref(node.data.child);
                    break;

                case NodeType::BreakStmt:
                    std::cout << std::setw(20) << "[BreakStmt]"
                              << "Label: " << node_ref(node.data.break_stmt.label)
                              << " | Expr: " << node_ref(node.data.break_stmt.value);
                    break;

                case NodeType::ContinueStmt:
                    std::cout << std::setw(20) << "[ContinueStmt]";
                    break;

                case NodeType::CallExpr:
                {
                    std::cout << std::setw(20) << "[CallExpr]";
                    uint32_t callee = node.data.call.callee;
                    uint32_t args   = node.data.call.args;
                    std::cout << "Callee: " << node_ref(callee);

                    if (args != ERROR_NODE && args < m_Nodes.size())
                    {
                        const auto& arg_list = m_Nodes[args];
                        std::cout << " | ";
                        dump_list(arg_list.data.list.start, arg_list.data.list.count, "Args");
                    }
                    break;
                }

                case NodeType::ArgList:
                {
                    std::cout << std::setw(20) << "[ArgList]";
                    dump_list(node.data.list.start, node.data.list.count, "Args");
                    break;
                }

                case NodeType::IfExpr:
                    std::cout << std::setw(20) << "[IfExpr]"
                              << "Cond: " << node_ref(node.data.if_expr.cond)
                              << " | Then: " << node_ref(node.data.if_expr.then_branch)
                              << " | Else: " << node_ref(node.data.if_expr.else_branch);
                    break;

                case NodeType::ExprStmt:
                    std::cout << std::setw(20) << "[ExprStmt]"
                              << "Expr: " << node_ref(node.data.child);
                    break;

                case NodeType::LabeledBlockExpr:
                    std::cout << std::setw(20) << "[LabeledBlockExpr]"
                              << "Label: " << node_ref(node.data.labeled.label)
                              << " | Body: " << node_ref(node.data.labeled.body);
                    break;

                case NodeType::BlockExpr:
                    std::cout << std::setw(20) << "[BlockExpr]";
                    dump_list(node.data.list.start, node.data.list.count, "Stmts");
                    break;

                case NodeType::Program:
                    std::cout << std::setw(20) << "[Program]"
                              << "Stmts: " << node.data.list.count;
                    break;

                case NodeType::FuncDecl:
                    std::cout << std::setw(20) << "[FuncDecl]"
                              << "Sign: " << node_ref(node.data.func_decl.sign)
                              << " | Body: " << node_ref(node.data.func_decl.body);
                    break;

                case NodeType::FuncSign:
                    std::cout << std::setw(20) << "[FuncSign]"
                              << "Name: " << node_ref(node.data.func_sign.name)
                              << " | Params: " << node_ref(node.data.func_sign.params)
                              << " | Ret: " << node_ref(node.data.func_sign.ret_type);
                    break;

                case NodeType::FuncArgDecl:
                    std::cout << std::setw(20) << "[FuncArgDecl]"
                              << "Name: " << node_ref(node.data.func_arg.ident)
                              << " | Type: " << node_ref(node.data.func_arg.type)
                              << " | Default: " << node_ref(node.data.func_arg.default_val);
                    break;

                case NodeType::StructDecl:
                {
                    std::cout << std::setw(20) << "[StructDecl]"
                              << "Name: " << node_ref(node.data.struct_decl.name);
                    uint32_t fields = node.data.struct_decl.fields;
                    if (fields != ERROR_NODE && fields < m_Nodes.size())
                    {
                        const auto& f = m_Nodes[fields];
                        std::cout << " | ";
                        dump_list(f.data.list.start, f.data.list.count, "Fields");
                    }
                    else
                    {
                        std::cout << " | Fields: (unit)";
                    }
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