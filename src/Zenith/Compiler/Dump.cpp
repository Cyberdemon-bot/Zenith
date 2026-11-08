#include <iostream>
#include <iomanip>
#include <format>
#include <sstream>
#include <string>
#include <string_view>
#include <cstdio>
#include "Zenith/Compiler/Parser.h"

namespace Zenith {

    std::string_view StringContent(std::string_view lit)
    {
        if (lit.size() >= 2 && lit.front() == '"' && lit.back() == '"')
            return lit.substr(1, lit.size() - 2);
        return lit;
    }

    std::string_view LabelName(std::string_view lit)
    {
        if (!lit.empty() && lit.front() == '\'') lit.remove_prefix(1);
        if (!lit.empty() && lit.back() == ':')    lit.remove_suffix(1);
        return lit;
    }

    std::string FmtF64(double v) { return std::format("{}", v); }

    std::string Parser::NodeTypeToString(NodeType type) const 
    {
        switch (type) {
            case NodeType::Integer: return "Integer";
            case NodeType::Decimal: return "Decimal";
            case NodeType::String: return "String";
            case NodeType::Bool: return "Bool";
            case NodeType::Null: return "Null";
            case NodeType::Tuple: return "Tuple";
            case NodeType::Identifier: return "Identifier";
            case NodeType::TypedIdentifier: return "TypedIdentifier";
            case NodeType::Label: return "Label";
            case NodeType::UnaryExpr: return "UnaryExpr";
            case NodeType::BinaryExpr: return "BinaryExpr";
            case NodeType::CastExpr: return "CastExpr";
            case NodeType::DerefExpr: return "DerefExpr";
            case NodeType::ParenExpr: return "ParenExpr";
            case NodeType::CallExpr: return "CallExpr";
            case NodeType::ArgList: return "ArgList";
            case NodeType::IndexExpr: return "IndexExpr";
            case NodeType::FieldAccess: return "FieldAccess";
            case NodeType::BorrowExpr: return "BorrowExpr";
            case NodeType::NewExpr: return "NewExpr";
            case NodeType::StructInit: return "StructInit";
            case NodeType::FieldInit: return "FieldInit";
            case NodeType::FieldInitList: return "FieldInitList";
            case NodeType::LabeledExpr: return "LabeledExpr";
            case NodeType::BlockExpr: return "BlockExpr";
            case NodeType::IfExpr: return "IfExpr";
            case NodeType::ForExpr: return "ForExpr";
            case NodeType::ForInExpr: return "ForInExpr";
            case NodeType::MatchExpr: return "MatchExpr";
            case NodeType::MatchArm: return "MatchArm";
            case NodeType::ExprStmt: return "ExprStmt";
            case NodeType::LetStmt: return "LetStmt";
            case NodeType::ReturnStmt: return "ReturnStmt";
            case NodeType::BreakStmt: return "BreakStmt";
            case NodeType::ContinueStmt: return "ContinueStmt";
            case NodeType::AssignStmt: return "AssignStmt";
            case NodeType::NamedType: return "NamedType";
            case NodeType::DynType:   return "DynType";
            case NodeType::RefType: return "RefType";
            case NodeType::TupleType: return "TupleType";
            case NodeType::TypeArgList: return "TypeArgList";
            case NodeType::FuncDecl: return "FuncDecl";
            case NodeType::FuncSign: return "FuncSign";
            case NodeType::StructDecl: return "StructDecl";
            case NodeType::ClassDecl: return "ClassDecl";
            case NodeType::Program: return "Program";
            default: return "Unknown";
        }
    }

    void Parser::DumpErrors(std::ostream& out) const 
    {
        if (m_Errors.empty()) 
        {
            out << "[Parser] No errors found.\n";
            return;
        }
        out << std::format("=== Found {} Parse Error(s) ===\n", m_Errors.size());
        const char* src_begin = m_Source.data();
        const char* src_end   = src_begin + m_Source.size();
        for (size_t i = 0; i < m_Errors.size(); ++i) 
        {
            const auto& err = m_Errors[i];
            uint32_t target_tok_idx = err.token;
            if (target_tok_idx >= m_Tokens.size())
                target_tok_idx = static_cast<uint32_t>(m_Tokens.size()) - 1;
            const Token& tok = m_Tokens[target_tok_idx];
            out << std::format("[Error #{}] {}\n", i + 1, err.message);
            out << std::format("  --> Line {}:{}\n", tok.line, tok.column);
            if (tok.literal.empty()) 
            {
                out << "\n";
                continue;
            }
            size_t highlight_len = std::max<size_t>(1, tok.literal.size());
            if (err.scope == ErrorScope::NodeStart &&
                err.node != ERROR_NODE && err.node < m_Nodes.size()) 
            {
                const auto& node = m_Nodes[err.node];
                if (node.start_token < m_Tokens.size() &&
                    node.end_token   < m_Tokens.size()) {
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
            out << "   |\n";
            out << std::format("{:>3} | {}\n", tok.line, line_snippet);
            out << std::format("   | {}{}\n",
                std::string(indent_cols, ' '),
                std::string(highlight_len, '^'));
        }
    }

    void Parser::DumpFlatAst(std::ostream& out) const
    {
        out << "\n================ FLAT AST DUMP ("
            << m_Nodes.size() << " nodes) ================\n";
        out << std::left
            << std::setw(10) << "NODE"
            << std::setw(30) << "TOKEN"
            << std::setw(20) << "TYPE"
            << "DETAILS\n";
        out << std::string(96, '-') << "\n";
        auto fmt_idx = [](uint32_t idx) -> std::string 
        {
            char buf[16];
            snprintf(buf, sizeof(buf), "%02u", idx);
            return std::string(buf);
        };
        auto node_ref = [&](uint32_t idx) -> std::string 
        {
            return (idx == ERROR_NODE) ? std::string("None")
                : "Node[" + fmt_idx(idx) + "]";
        };
        auto tok_ref = [&](uint32_t idx) -> std::string 
        {
            if (idx >= m_Tokens.size()) return std::string("None");
            return "Tok[" + fmt_idx(idx) + "]";
        };
        auto dump_list = [&](uint32_t start, uint32_t count,
            std::string_view label) {
            out << label << ": [";
            for (uint32_t c = 0; c < count; ++c) {
                if (c > 0) out << ", ";
                uint32_t e = m_Extra[start + c];
                out << (e == ERROR_NODE ? "None" : "Node[" + fmt_idx(e) + "]");
            }
            out << "]";
        };
        for (size_t i = 1; i < m_Nodes.size(); ++i) 
        {
            const auto& node = m_Nodes[i];
            const auto& tok  = m_Tokens[node.start_token];
            std::string node_str = "Node[" + fmt_idx(static_cast<uint32_t>(i)) + "]";
            out << std::left << std::setw(10) << node_str;
            std::string tok_str = "Tok[" + fmt_idx(node.start_token) + "]: '"
                + std::string(tok.literal) + "'";
            out << std::left << std::setw(30) << tok_str << (tok_str.size() >= 30 ? " " : "");
            switch (node.type) {
            case NodeType::Integer:
                out << std::setw(20) << "[Integer]" << "Val: " << node.data.i64;
                break;
            case NodeType::Decimal:
                out << std::setw(20) << "[Decimal]" << "Val: " << FmtF64(node.data.f64);
                break;
            case NodeType::String:
                out << std::setw(20) << "[String]" << "Val: \"" << tok.literal << "\"";
                break;
            case NodeType::Bool:
                out << std::setw(20) << "[Bool]" << "Val: " << (node.data.i64 ? "true" : "false");
                break;
            case NodeType::Null:
                out << std::setw(20) << "[Null]";
                break;
            case NodeType::Identifier:
                out << std::setw(20) << "[Identifier]" << "Name: " << tok.literal;
                break;
            case NodeType::TypedIdentifier:
                out << std::setw(20) << "[TypedIdentifier]"
                    << "Name: " << node_ref(node.data.typed_ident.ident)
                    << " | Type: " << node_ref(node.data.typed_ident.type)
                    << " | Mut: "  << (node.data.typed_ident.is_mut ? "yes" : "no");
                break;
            case NodeType::Label:
                out << std::setw(20) << "[Label]" << "Name: " << LabelName(tok.literal);
                break;
            case NodeType::UnaryExpr:
                out << std::setw(20) << "[UnaryExpr]"
                    << "Op: "    << tok_ref(node.data.unary.op)
                    << " | Child: " << node_ref(node.data.unary.operand);
                break;
            case NodeType::BinaryExpr:
            case NodeType::AssignStmt:
            {
                uint32_t op_tok = node.data.binary.op;
                std::string_view op_str =
                    (op_tok < m_Tokens.size()) ? m_Tokens[op_tok].literal : std::string_view{};
                out << std::setw(20) << (node.type == NodeType::BinaryExpr ? "[BinaryExpr]" : "[AssignStmt]")
                    << "LHS: " << node_ref(node.data.binary.lhs)
                    << " | Op: '" << op_str << "'"
                    << " | RHS: " << node_ref(node.data.binary.rhs);
                break;
            }
            case NodeType::CastExpr:
                out << std::setw(20) << "[CastExpr]"
                    << "Expr: " << node_ref(node.data.cast.expr)
                    << " | Type: " << node_ref(node.data.cast.type);
                break;
            case NodeType::FieldAccess:
                out << std::setw(20) << "[FieldAccess]"
                    << "Obj: "   << node_ref(node.data.access.target)
                    << " | Field: " << node_ref(node.data.access.member);
                break;
            case NodeType::IndexExpr:
                out << std::setw(20) << "[IndexExpr]"
                    << "Arr: "   << node_ref(node.data.access.target)
                    << " | Index: " << node_ref(node.data.access.member);
                break;
            case NodeType::BorrowExpr:
                out << std::setw(20) << "[BorrowExpr]"
                    << "Mut: "     << (node.data.borrow.is_mut ? "yes" : "no")
                    << " | Target: " << node_ref(node.data.borrow.operand);
                break;
            case NodeType::NamedType:
            {
                uint32_t ident_tok = node.data.named_type.ident;
                std::string_view type_name =
                    (ident_tok < m_Tokens.size()) ? m_Tokens[ident_tok].literal : std::string_view{};
                out << std::setw(20) << "[NamedType]"
                    << "Name: " << type_name
                    << " | Args: " << node_ref(node.data.named_type.args);
                break;
            }
            case NodeType::RefType:
                out << std::setw(20) << "[RefType]"
                    << "Mut: " << (node.data.ref_type.is_mut ? "yes" : "no")
                    << " | Inner: " << node_ref(node.data.ref_type.inner);
                break;
            case NodeType::DynType:
                out << std::setw(20) << "[DynType]"
                    << "Inner: " << node_ref(node.data.child);
                break;
            case NodeType::TupleType:
                out << std::setw(20) << "[TupleType]";
                dump_list(node.data.list.start, node.data.list.count, "Elems");
                break;
            case NodeType::TypeArgList:
                out << std::setw(20) << "[TypeArgList]";
                dump_list(node.data.list.start, node.data.list.count, "Types");
                break;
            case NodeType::Tuple:
                out << std::setw(20) << "[Tuple]";
                dump_list(node.data.list.start, node.data.list.count, "Elems");
                break;
            case NodeType::ParenExpr:
                out << std::setw(20) << "[ParenExpr]" << "Expr: " << node_ref(node.data.child);
                break;
            case NodeType::DerefExpr:
                out << std::setw(20) << "[DerefExpr]" << "Expr: " << node_ref(node.data.child);
                break;
            case NodeType::NewExpr:
                out << std::setw(20) << "[NewExpr]"
                    << "Mut: " << (node.data.new_expr.is_mut ? "yes" : "no")
                    << " | Type: " << node_ref(node.data.new_expr.type)
                    << " | Args: " << node_ref(node.data.new_expr.args);
                break;
            case NodeType::StructInit:
                out << std::setw(20) << "[StructInit]"
                    << "Name: " << node_ref(node.data.pair.first)
                    << " | Fields: " << node_ref(node.data.pair.second);
                break;
            case NodeType::FieldInit:
                out << std::setw(20) << "[FieldInit]"
                    << "Name: " << node_ref(node.data.pair.first)
                    << " | Value: " << node_ref(node.data.pair.second);
                break;
            case NodeType::FieldInitList:
                out << std::setw(20) << "[FieldInitList]";
                dump_list(node.data.list.start, node.data.list.count, "Fields");
                break;
            case NodeType::LetStmt:
                out << std::setw(20) << "[LetStmt]"
                    << "Target: " << node_ref(node.data.let.target)
                    << " | Init: " << node_ref(node.data.let.value);
                break;
            case NodeType::ReturnStmt:
                out << std::setw(20) << "[ReturnStmt]" << "Expr: " << node_ref(node.data.child);
                break;
            case NodeType::BreakStmt:
                out << std::setw(20) << "[BreakStmt]"
                    << "Label: " << node_ref(node.data.break_stmt.label)
                    << " | Expr: " << node_ref(node.data.break_stmt.value);
                break;
            case NodeType::ContinueStmt:
                out << std::setw(20) << "[ContinueStmt]"
                    << "Label: " << node_ref(node.data.continue_stmt.label);
                break;
            case NodeType::CallExpr:
            {
                out << std::setw(20) << "[CallExpr]";
                uint32_t callee = node.data.call.callee;
                uint32_t args   = node.data.call.args;
                out << "Callee: " << node_ref(callee);
                if (args != ERROR_NODE && args < m_Nodes.size()) {
                    const auto& arg_list = m_Nodes[args];
                    out << " | ";
                    dump_list(arg_list.data.list.start, arg_list.data.list.count, "Args");
                }
                break;
            }
            case NodeType::ArgList:
            {
                out << std::setw(20) << "[ArgList]";
                dump_list(node.data.list.start, node.data.list.count, "Args");
                break;
            }
            case NodeType::IfExpr:
                out << std::setw(20) << "[IfExpr]"
                    << "Cond: " << node_ref(node.data.if_expr.cond)
                    << " | Then: " << node_ref(node.data.if_expr.then_branch)
                    << " | Else: " << node_ref(node.data.if_expr.else_branch);
                break;
            case NodeType::ExprStmt:
                out << std::setw(20) << "[ExprStmt]" << "Expr: " << node_ref(node.data.child);
                break;
            case NodeType::LabeledExpr:
                out << std::setw(20) << "[LabeledExpr]"
                    << "Label: " << node_ref(node.data.labeled.label)
                    << " | Body: " << node_ref(node.data.labeled.body);
                break;
            case NodeType::BlockExpr:
                out << std::setw(20) << "[BlockExpr]";
                dump_list(node.data.list.start, node.data.list.count, "Stmts");
                break;
            case NodeType::Program:
                out << std::setw(20) << "[Program]";
                dump_list(node.data.list.start, node.data.list.count, "Stmts");
                break;
            case NodeType::FuncDecl:
                out << std::setw(20) << "[FuncDecl]"
                    << "Sign: " << node_ref(node.data.func_decl.sign)
                    << " | Body: " << node_ref(node.data.func_decl.body)
                    << " | Pub: " << (node.data.func_decl.is_pub ? "yes" : "no");
                break;
            case NodeType::FuncSign:
                out << std::setw(20) << "[FuncSign]"
                    << "Name: " << node_ref(node.data.func_sign.name)
                    << " | Params: " << node_ref(node.data.func_sign.params)
                    << " | Ret: " << node_ref(node.data.func_sign.ret_type);
                break;
            case NodeType::StructDecl:
            {
                out << std::setw(20) << "[StructDecl]" << "Name: " << node_ref(node.data.struct_decl.name);
                uint32_t fields = node.data.struct_decl.fields;
                if (fields != ERROR_NODE && fields < m_Nodes.size()) {
                    const auto& f = m_Nodes[fields];
                    out << " | ";
                    dump_list(f.data.list.start, f.data.list.count, "Fields");
                }
                else out << " | Fields: (unit)";
                out << " | Pub: " << (node.data.struct_decl.is_pub ? "yes" : "no");
                break;
            }
            case NodeType::ForExpr:
                out << std::setw(20) << "[ForExpr]"
                    << "Cond: " << node_ref(node.data.for_expr.cond)
                    << " | Body: " << node_ref(node.data.for_expr.body);
                break;
            case NodeType::ForInExpr:
                out << std::setw(20) << "[ForInExpr]"
                    << "Var: " << node_ref(node.data.for_in.var)
                    << " | Iterable: " << node_ref(node.data.for_in.iterable)
                    << " | Body: " << node_ref(node.data.for_in.body);
                break;
            default:
                out << std::setw(20)
                    << ("[Unknown:" +
                        std::to_string(static_cast<uint32_t>(node.type)) +
                        "]");
                break;
            }
            out << "\n";
        }
        out << std::string(96, '=') << "\n";
    }

    void Parser::DumpTreeList(std::ostream& out, uint32_t start, uint32_t count, int depth) const
    {
        for (uint32_t i = 0; i < count; ++i) 
            DumpTreeNode(out, m_Extra[start + i], depth);
    }

    void Parser::DumpTreeNode(std::ostream& out, uint32_t idx, int depth) const 
    {
        std::string pad(depth * 2, ' ');
        
        if (idx == ERROR_NODE || idx >= m_Nodes.size()) 
        {
            out << pad << "(Error)\n";
            return;
        }

        const auto& node = m_Nodes[idx];
        const auto& tok  = m_Tokens[node.start_token];
        
        out << pad << "(" << NodeTypeToString(node.type);
        switch (node.type) {
            case NodeType::Integer: out << " " << node.data.i64; break;
            case NodeType::Decimal: out << " " << FmtF64(node.data.f64); break;
            case NodeType::String:  out << " \"" << tok.literal << "\""; break;
            case NodeType::Bool:    out << " " << (node.data.i64 ? "true" : "false"); break;
            case NodeType::Null:    break;
            case NodeType::Identifier: out << " \"" << tok.literal << "\""; break;
            case NodeType::Label:   out << " \"" << LabelName(tok.literal) << "\""; break;
            default: break;
        }

        bool has_children = false;
        auto print_child = [&](uint32_t child_idx) 
        {
            if (child_idx != ERROR_NODE) 
            {
                if (!has_children) { out << "\n"; has_children = true; }
                DumpTreeNode(out, child_idx, depth + 1);
            }
        };

        auto print_list = [&](uint32_t start, uint32_t count) 
        {
            if (count > 0) 
            {
                if (!has_children) { out << "\n"; has_children = true; }
                DumpTreeList(out, start, count, depth + 1);
            }
        };

        switch (node.type) 
        {
            case NodeType::UnaryExpr:
                out << " :op \"" << m_Tokens[node.data.unary.op].literal << "\"";
                print_child(node.data.unary.operand);
                break;
            case NodeType::BinaryExpr:
            case NodeType::AssignStmt:
                out << " :op \"" << m_Tokens[node.data.binary.op].literal << "\"";
                print_child(node.data.binary.lhs);
                print_child(node.data.binary.rhs);
                break;
            case NodeType::CastExpr:
                print_child(node.data.cast.expr);
                print_child(node.data.cast.type);
                break;
            case NodeType::FieldAccess:
            case NodeType::IndexExpr:
                print_child(node.data.access.target);
                print_child(node.data.access.member);
                break;
            case NodeType::BorrowExpr:
                out << " :mut " << (node.data.borrow.is_mut ? "true" : "false");
                print_child(node.data.borrow.operand);
                break;
            case NodeType::DerefExpr:
            case NodeType::ParenExpr:
            case NodeType::DynType:
            case NodeType::ExprStmt:
            case NodeType::ReturnStmt:
                print_child(node.data.child);
                break;
            case NodeType::CallExpr:
                print_child(node.data.call.callee);
                print_child(node.data.call.args);
                break;
            case NodeType::IfExpr:
                print_child(node.data.if_expr.cond);
                print_child(node.data.if_expr.then_branch);
                print_child(node.data.if_expr.else_branch);
                break;
            case NodeType::ForExpr:
                print_child(node.data.for_expr.cond);
                print_child(node.data.for_expr.body);
                break;
            case NodeType::ForInExpr:
                print_child(node.data.for_in.var);
                print_child(node.data.for_in.iterable);
                print_child(node.data.for_in.body);
                break;
            case NodeType::LetStmt:
                print_child(node.data.let.target);
                print_child(node.data.let.value);
                break;
            case NodeType::TypedIdentifier:
                out << " :mut " << (node.data.typed_ident.is_mut ? "true" : "false");
                print_child(node.data.typed_ident.ident);
                print_child(node.data.typed_ident.type);
                break;
            case NodeType::FuncDecl:
                if (node.data.func_decl.is_pub) out << " :pub true";
                print_child(node.data.func_decl.sign);
                print_child(node.data.func_decl.body);
                break;
            case NodeType::FuncSign:
                print_child(node.data.func_sign.name);
                print_child(node.data.func_sign.params);
                print_child(node.data.func_sign.ret_type);
                break;
            case NodeType::StructDecl:
                if (node.data.struct_decl.is_pub) out << " :pub true";
                print_child(node.data.struct_decl.name);
                print_child(node.data.struct_decl.fields);
                break;
            case NodeType::NewExpr:
                out << " :mut " << (node.data.new_expr.is_mut ? "true" : "false");
                print_child(node.data.new_expr.type);
                print_child(node.data.new_expr.args);
                break;
            case NodeType::StructInit:
            case NodeType::FieldInit:
                print_child(node.data.pair.first);
                print_child(node.data.pair.second);
                break;
            case NodeType::BreakStmt:
                if (node.data.break_stmt.label != ERROR_NODE || node.data.break_stmt.value != ERROR_NODE) {
                    print_child(node.data.break_stmt.label);
                    print_child(node.data.break_stmt.value);
                }
                break;
            case NodeType::ContinueStmt:
                if (node.data.continue_stmt.label != ERROR_NODE) {
                    print_child(node.data.continue_stmt.label);
                }
                break;
            case NodeType::LabeledExpr:
                print_child(node.data.labeled.label);
                print_child(node.data.labeled.body);
                break;
            case NodeType::MatchArm:
                print_child(node.data.match_arm.pattern);
                print_child(node.data.match_arm.guard);
                print_child(node.data.match_arm.body);
                break;
            case NodeType::Program:
            case NodeType::BlockExpr:
            case NodeType::ArgList:
            case NodeType::FieldInitList:
            case NodeType::Tuple:
            case NodeType::TupleType:
            case NodeType::TypeArgList:
                print_list(node.data.list.start, node.data.list.count);
                break;
            case NodeType::NamedType:
                out << " \"" << m_Tokens[node.data.named_type.ident].literal << "\"";
                print_child(node.data.named_type.args);
                break;
            case NodeType::RefType:
                out << " :mut " << (node.data.ref_type.is_mut ? "true" : "false");
                print_child(node.data.ref_type.inner);
                break;
            default:
                break;
        }
        
        if (has_children) out << pad;
        out << ")\n";
    }

    void Parser::DumpTreeAst(std::ostream& out, uint32_t rootIdx) const 
    {
        if (rootIdx == ERROR_NODE || rootIdx >= m_Nodes.size() || m_Nodes[rootIdx].type != NodeType::Program) 
        {
            for (int32_t i = (int32_t)m_Nodes.size() - 1; i >= 1; --i) 
            {
                if (m_Nodes[i].type == NodeType::Program) 
                {
                    rootIdx = i;
                    break;
                }
            }
        }
        
        if (rootIdx == ERROR_NODE || rootIdx >= m_Nodes.size()) 
        {
            out << "(Error: No Program node found)\n";
            return;
        }
        
        DumpTreeNode(out, rootIdx, 0);
    }
}
