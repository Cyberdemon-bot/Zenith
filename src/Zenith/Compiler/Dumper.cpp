#include "Zenith/Compiler/Dumper.h"
#include "Zenith/Compiler/Utils.h"

#include <algorithm>
#include <format>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

namespace Zenith {

    namespace {

        std::string_view LabelName(std::string_view lit)
        {
            if (!lit.empty() && lit.front() == '\'') lit.remove_prefix(1);
            if (!lit.empty() && lit.back() == ':')    lit.remove_suffix(1);
            return lit;
        }

        std::string FmtF64(double v) { return std::format("{}", v); }

        std::string_view NodeTypeName(NodeType type)
        {
            switch (type)
            {
                case NodeType::Integer:         return "Integer";
                case NodeType::Decimal:         return "Decimal";
                case NodeType::String:          return "String";
                case NodeType::Bool:            return "Bool";
                case NodeType::Null:            return "Null";
                case NodeType::Tuple:           return "Tuple";
                case NodeType::Identifier:      return "Identifier";
                case NodeType::TypedIdentifier: return "TypedIdentifier";
                case NodeType::Label:           return "Label";
                case NodeType::UnaryExpr:       return "UnaryExpr";
                case NodeType::BinaryExpr:      return "BinaryExpr";
                case NodeType::CastExpr:        return "CastExpr";
                case NodeType::DerefExpr:       return "DerefExpr";
                case NodeType::ParenExpr:       return "ParenExpr";
                case NodeType::CallExpr:        return "CallExpr";
                case NodeType::ArgList:         return "ArgList";
                case NodeType::IndexExpr:       return "IndexExpr";
                case NodeType::FieldAccess:     return "FieldAccess";
                case NodeType::BorrowExpr:      return "BorrowExpr";
                case NodeType::NewExpr:         return "NewExpr";
                case NodeType::StructInit:      return "StructInit";
                case NodeType::FieldInit:       return "FieldInit";
                case NodeType::FieldInitList:   return "FieldInitList";
                case NodeType::LabeledExpr:     return "LabeledExpr";
                case NodeType::BlockExpr:       return "BlockExpr";
                case NodeType::IfExpr:          return "IfExpr";
                case NodeType::ForExpr:         return "ForExpr";
                case NodeType::ForInExpr:       return "ForInExpr";
                case NodeType::MatchExpr:       return "MatchExpr";
                case NodeType::MatchArm:        return "MatchArm";
                case NodeType::ExprStmt:        return "ExprStmt";
                case NodeType::LetStmt:         return "LetStmt";
                case NodeType::ReturnStmt:      return "ReturnStmt";
                case NodeType::BreakStmt:       return "BreakStmt";
                case NodeType::ContinueStmt:    return "ContinueStmt";
                case NodeType::AssignStmt:      return "AssignStmt";
                case NodeType::NamedType:       return "NamedType";
                case NodeType::DynType:         return "DynType";
                case NodeType::RefType:         return "RefType";
                case NodeType::TupleType:       return "TupleType";
                case NodeType::TypeArgList:     return "TypeArgList";
                case NodeType::FuncDecl:        return "FuncDecl";
                case NodeType::FuncSign:        return "FuncSign";
                case NodeType::StructDecl:      return "StructDecl";
                case NodeType::ClassDecl:       return "ClassDecl";
                case NodeType::Program:         return "Program";
                default:                        return "Unknown";
            }
        }

        void AppendSourceSnippet(
            std::ostringstream& out,
            AstView ast,
            uint32_t token_idx,
            uint32_t node_idx,
            bool use_node_span)
        {
            if (ast.tokens.empty()) return;

            uint32_t target_tok_idx = token_idx;
            if (target_tok_idx >= ast.tokens.size())
                target_tok_idx = static_cast<uint32_t>(ast.tokens.size()) - 1;

            const Token& tok = ast.tokens[target_tok_idx];
            out << std::format("  --> Line {}:{}\n", tok.line, tok.column);
            if (tok.literal.empty())
            {
                out << "\n";
                return;
            }

            size_t highlight_len = std::max<size_t>(1, tok.literal.size());
            if (use_node_span && node_idx != ERROR_NODE && node_idx < ast.nodes.size())
            {
                const auto& node = ast.nodes[node_idx];
                if (node.start_token < ast.tokens.size() && node.end_token < ast.tokens.size())
                {
                    const char* start_ptr = ast.tokens[node.start_token].literal.data();
                    const char* end_ptr   = ast.tokens[node.end_token].literal.data()
                                          + ast.tokens[node.end_token].literal.size();
                    if (end_ptr >= start_ptr)
                        highlight_len = std::max<size_t>(1, static_cast<size_t>(end_ptr - start_ptr));
                }
            }

            const char* src_begin = ast.source.data();
            const char* src_end   = src_begin + ast.source.size();
            const char* tok_begin = tok.literal.data();

            const char* line_start = tok_begin;
            while (line_start > src_begin && line_start[-1] != '\n' && line_start[-1] != '\r')
                --line_start;

            const char* line_end = tok_begin;
            while (line_end < src_end && *line_end != '\n' && *line_end != '\r')
                ++line_end;

            std::string_view line_snippet(line_start, static_cast<size_t>(line_end - line_start));
            size_t indent_cols     = static_cast<size_t>(tok_begin - line_start);
            size_t max_line_remain = static_cast<size_t>(line_end - tok_begin);
            highlight_len = std::max<size_t>(1, std::min(highlight_len, max_line_remain));

            out << "   |\n";
            out << std::format("{:>3} | {}\n", tok.line, line_snippet);
            out << std::format("   | {}{}\n",
                std::string(indent_cols, ' '),
                std::string(highlight_len, '^'));
        }

        uint32_t FindProgramRoot(AstView ast, uint32_t root_idx)
        {
            if (root_idx != ERROR_NODE && root_idx < ast.nodes.size() && ast.nodes[root_idx].type == NodeType::Program)
                return root_idx;

            for (int32_t i = static_cast<int32_t>(ast.nodes.size()) - 1; i >= 1; --i)
            {
                if (ast.nodes[i].type == NodeType::Program)
                    return static_cast<uint32_t>(i);
            }
            return ERROR_NODE;
        }

        void DumpTreeNode(
            std::ostringstream& out,
            AstView ast,
            std::span<const CTType> node_types,
            uint32_t idx,
            int depth);

        void DumpTreeList(
            std::ostringstream& out,
            AstView ast,
            std::span<const CTType> node_types,
            uint32_t start,
            uint32_t count,
            int depth)
        {
            for (uint32_t i = 0; i < count; ++i)
                DumpTreeNode(out, ast, node_types, ast.extra[start + i], depth);
        }

        void DumpTreeNode(
            std::ostringstream& out,
            AstView ast,
            std::span<const CTType> node_types,
            uint32_t idx,
            int depth)
        {
            std::string pad(depth * 2, ' ');

            if (idx == ERROR_NODE || idx >= ast.nodes.size())
            {
                out << pad << "(Error)\n";
                return;
            }

            const auto& node = ast.nodes[idx];
            const auto& tok  = ast.tokens[node.start_token];

            out << pad << "(" << NodeTypeName(node.type);
            switch (node.type)
            {
                case NodeType::Integer:    out << " " << node.data.i64; break;
                case NodeType::Decimal:    out << " " << FmtF64(node.data.f64); break;
                case NodeType::String:     out << " \"" << tok.literal << "\""; break;
                case NodeType::Bool:       out << " " << (node.data.i64 ? "true" : "false"); break;
                case NodeType::Identifier: out << " \"" << tok.literal << "\""; break;
                case NodeType::Label:      out << " \"" << LabelName(tok.literal) << "\""; break;
                default: break;
            }

            bool has_children = false;
            auto print_child = [&](uint32_t child_idx)
            {
                if (child_idx != ERROR_NODE)
                {
                    if (!has_children) { out << "\n"; has_children = true; }
                    DumpTreeNode(out, ast, node_types, child_idx, depth + 1);
                }
            };

            auto print_list = [&](uint32_t start, uint32_t count)
            {
                if (count > 0)
                {
                    if (!has_children) { out << "\n"; has_children = true; }
                    DumpTreeList(out, ast, node_types, start, count, depth + 1);
                }
            };

            auto append_type_attr = [&]()
            {
                if (!node_types.empty() && idx < node_types.size() && node_types[idx] != CTType::Unknown())
                    out << " :type " << Utils::TypeToString(node_types[idx]);
            };

            switch (node.type)
            {
                case NodeType::UnaryExpr:
                    out << " :op \"" << OperatorToString(node.data.unary.op) << "\"";
                    append_type_attr();
                    print_child(node.data.unary.operand);
                    break;
                case NodeType::BinaryExpr:
                    out << " :op \"" << OperatorToString(node.data.binary.op) << "\"";
                    append_type_attr();
                    print_child(node.data.binary.lhs);
                    print_child(node.data.binary.rhs);
                    break;
                case NodeType::AssignStmt:
                    append_type_attr();
                    print_child(node.data.assign.target);
                    print_child(node.data.assign.value);
                    break;
                case NodeType::CastExpr:
                    append_type_attr();
                    print_child(node.data.cast.expr);
                    print_child(node.data.cast.type);
                    break;
                case NodeType::FieldAccess:
                case NodeType::IndexExpr:
                    append_type_attr();
                    print_child(node.data.access.target);
                    print_child(node.data.access.member);
                    break;
                case NodeType::BorrowExpr:
                    out << " :mut " << (node.data.borrow.is_mut ? "true" : "false");
                    append_type_attr();
                    print_child(node.data.borrow.operand);
                    break;
                case NodeType::DerefExpr:
                case NodeType::ParenExpr:
                case NodeType::DynType:
                case NodeType::ExprStmt:
                case NodeType::ReturnStmt:
                    append_type_attr();
                    print_child(node.data.child);
                    break;
                case NodeType::CallExpr:
                    append_type_attr();
                    print_child(node.data.call.callee);
                    print_child(node.data.call.args);
                    break;
                case NodeType::IfExpr:
                    append_type_attr();
                    print_child(node.data.if_expr.cond);
                    print_child(node.data.if_expr.then_branch);
                    print_child(node.data.if_expr.else_branch);
                    break;
                case NodeType::ForExpr:
                    append_type_attr();
                    print_child(node.data.for_expr.cond);
                    print_child(node.data.for_expr.body);
                    break;
                case NodeType::ForInExpr:
                    append_type_attr();
                    print_child(node.data.for_in.var);
                    print_child(node.data.for_in.iterable);
                    print_child(node.data.for_in.body);
                    break;
                case NodeType::LetStmt:
                    append_type_attr();
                    print_child(node.data.let.target);
                    print_child(node.data.let.value);
                    break;
                case NodeType::TypedIdentifier:
                    out << " :mut " << (node.data.typed_ident.is_mut ? "true" : "false");
                    append_type_attr();
                    print_child(node.data.typed_ident.ident);
                    print_child(node.data.typed_ident.type);
                    break;
                case NodeType::FuncDecl:
                    if (node.data.func_decl.is_pub) out << " :pub true";
                    append_type_attr();
                    print_child(node.data.func_decl.sign);
                    print_child(node.data.func_decl.body);
                    break;
                case NodeType::FuncSign:
                    append_type_attr();
                    print_child(node.data.func_sign.name);
                    print_child(node.data.func_sign.params);
                    print_child(node.data.func_sign.ret_type);
                    break;
                case NodeType::StructDecl:
                    if (node.data.struct_decl.is_pub) out << " :pub true";
                    append_type_attr();
                    print_child(node.data.struct_decl.name);
                    print_child(node.data.struct_decl.fields);
                    break;
                case NodeType::NewExpr:
                    out << " :mut " << (node.data.new_expr.is_mut ? "true" : "false");
                    append_type_attr();
                    print_child(node.data.new_expr.type);
                    print_child(node.data.new_expr.args);
                    break;
                case NodeType::StructInit:
                case NodeType::FieldInit:
                    append_type_attr();
                    print_child(node.data.pair.first);
                    print_child(node.data.pair.second);
                    break;
                case NodeType::BreakStmt:
                    append_type_attr();
                    if (node.data.break_stmt.label != ERROR_NODE || node.data.break_stmt.value != ERROR_NODE)
                    {
                        print_child(node.data.break_stmt.label);
                        print_child(node.data.break_stmt.value);
                    }
                    break;
                case NodeType::ContinueStmt:
                    append_type_attr();
                    if (node.data.continue_stmt.label != ERROR_NODE)
                        print_child(node.data.continue_stmt.label);
                    break;
                case NodeType::LabeledExpr:
                    append_type_attr();
                    print_child(node.data.labeled.label);
                    print_child(node.data.labeled.body);
                    break;
                case NodeType::MatchArm:
                    append_type_attr();
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
                    append_type_attr();
                    print_list(node.data.list.start, node.data.list.count);
                    break;
                case NodeType::NamedType:
                    out << " \"" << ast.tokens[node.data.named_type.ident].literal << "\"";
                    append_type_attr();
                    print_child(node.data.named_type.args);
                    break;
                case NodeType::RefType:
                    out << " :mut " << (node.data.ref_type.is_mut ? "true" : "false");
                    append_type_attr();
                    print_child(node.data.ref_type.inner);
                    break;
                default:
                    append_type_attr();
                    break;
            }

            if (has_children) out << pad;
            out << ")\n";
        }

        constexpr std::string_view GetDiagTemplate(DiagType type) noexcept
        {
            switch (type)
            {
                case DiagType::DuplicateFunction:    return "Duplicate function '{0}'";
                case DiagType::MissingMainFunction:  return "Program must define 'func main'";
                case DiagType::InvalidMainSignature: return "Invalid signature for 'main'";
                case DiagType::NestedFunction:       return "Nested functions are not supported";
                case DiagType::MissingParamType:     return "Parameter requires type annotation in Phase 3";
                case DiagType::UnknownTypeName:      return "Unknown type '{0}'";
                case DiagType::UnknownIdentifier:    return "Unknown identifier '{0}'";
                case DiagType::RedeclaredIdentifier: return "Redeclared identifier '{0}'";
                case DiagType::OperandTypeMismatch:  return "Invalid operand type '{actual}' for operator '{0}'";
                case DiagType::InvalidUnaryOp:       return "Unsupported unary operator '{0}'";
                case DiagType::InvalidBinaryOp:      return "Operands of '{0}' must have the same type (got '{expected}' and '{actual}')";
                case DiagType::WrongArgCount:        return "Argument count mismatch: expected {0}, got {1}";
                case DiagType::CannotInferType:      return "Cannot infer type";
                case DiagType::CoerceFailure:        return "Invalid type cast from '{actual}' to '{expected}'";
                case DiagType::LiteralOutOfRange:    return "Literal out of range for '{expected}'";
                case DiagType::LetTypeMismatch:      return "Let binding type mismatch: expected '{expected}', got '{actual}'";
                case DiagType::ReturnTypeMismatch:   return "Return type mismatch: expected '{expected}', got '{actual}'";
                case DiagType::ArgTypeMismatch:      return "Argument type mismatch: expected '{expected}', got '{actual}'";
                case DiagType::AssignTypeMismatch:   return "Assignment type mismatch: expected '{expected}', got '{actual}'";
                case DiagType::BreakTypeMismatch:    return "Break value type mismatch: expected '{expected}', got '{actual}'";
                case DiagType::MissingReturn:        return "Function '{0}' must return a value";
                case DiagType::ImmutableAssignment:  return "Cannot assign twice to immutable variable '{0}'";
                case DiagType::InvalidAssignTarget:  return "Invalid assignment target";
                case DiagType::UndeclaredLabel:      return "Undeclared label '{0}'";
                case DiagType::InvalidContinue:      return "'continue' must target an enclosing loop";
                case DiagType::BreakOutsideLoop:     return "'break' outside of loop or block";
                case DiagType::BlockMissingBreak:    return "Block does not break with a value of expected type";
                case DiagType::UnsupportedFeature:   return "Unsupported feature: {0}";
                case DiagType::TooManyErrors:        return "Too many errors";
                default:                             return "Unknown error";
            }
        }

    }

    std::string FormatDiagnostic(const Diagnostic& diag)
    {
        std::string_view tmpl = (diag.diag == DiagType::OperandTypeMismatch && diag.args.empty())
            ? std::string_view("Type mismatch: expected '{expected}', got '{actual}'")
            : GetDiagTemplate(diag.diag);

        std::string_view args[4]{};
        size_t arg_count = 0;
        if (!diag.args.empty())
        {
            size_t start = 0;
            while (arg_count < 4)
            {
                size_t pos = diag.args.find('\x1f', start);
                if (pos == std::string::npos)
                {
                    args[arg_count++] = std::string_view(diag.args).substr(start);
                    break;
                }
                args[arg_count++] = std::string_view(diag.args).substr(start, pos - start);
                start = pos + 1;
            }
        }

        std::string out;
        out.reserve(tmpl.size() + 32);
        for (size_t i = 0; i < tmpl.size(); ++i)
        {
            if (tmpl[i] == '{')
            {
                size_t close = tmpl.find('}', i + 1);
                if (close != std::string_view::npos)
                {
                    std::string_view key = tmpl.substr(i + 1, close - i - 1);
                    if (key == "expected")      out += Utils::TypeToString(diag.expected);
                    else if (key == "actual")   out += Utils::TypeToString(diag.actual);
                    else if (key.size() == 1 && key[0] >= '0' && key[0] <= '3')
                    {
                        size_t idx = static_cast<size_t>(key[0] - '0');
                        if (idx < arg_count) out += args[idx];
                    }
                    i = close;
                    continue;
                }
            }
            out += tmpl[i];
        }
        return out;
    }

    std::string Dumper::DumpParseErrors(AstView ast, std::span<const ParseError> errors)
    {
        if (errors.empty())
            return "[Parser] No errors found.\n";

        std::ostringstream out;
        out << std::format("=== Found {} Parse Error(s) ===\n", errors.size());

        for (size_t i = 0; i < errors.size(); ++i)
        {
            const auto& err = errors[i];
            out << std::format("[Error #{}] {}\n", i + 1, err.message);
            AppendSourceSnippet(out, ast, err.token, err.node, err.scope == ErrorScope::NodeStart);
        }
        return out.str();
    }

    std::string Dumper::DumpTreeAst(AstView ast, uint32_t root_idx)
    {
        std::ostringstream out;
        uint32_t resolved_root = FindProgramRoot(ast, root_idx);
        if (resolved_root == ERROR_NODE)
            return "(Error: No Program node found)\n";

        DumpTreeNode(out, ast, {}, resolved_root, 0);
        return out.str();
    }

    std::string Dumper::DumpTypedAst(AstView ast, std::span<const CTType> node_types, uint32_t root_idx)
    {
        std::ostringstream out;
        uint32_t resolved_root = FindProgramRoot(ast, root_idx);
        if (resolved_root == ERROR_NODE)
            return "(Error: No Program node found)\n";

        DumpTreeNode(out, ast, node_types, resolved_root, 0);
        return out.str();
    }

    std::string Dumper::DumpTypeErrors(AstView ast, std::span<const Diagnostic> errors)
    {
        if (errors.empty())
            return "[TypeChecker] No errors found.\n";

        std::ostringstream out;
        out << std::format("=== Found {} Type Error(s) ===\n", errors.size());

        for (size_t i = 0; i < errors.size(); ++i)
        {
            const auto& err = errors[i];
            out << std::format("[Error #{}] {}\n", i + 1, FormatDiagnostic(err));
            AppendSourceSnippet(out, ast, err.token, err.node, true);
        }
        return out.str();
    }

    std::string Dumper::DumpTypes(AstView ast, std::span<const CTType> node_types)
    {
        if (node_types.size() <= 1)
            return "[TypeChecker] No node types recorded.\n";

        std::ostringstream out;
        out << "\n================ NODE TYPES DUMP ("
            << (node_types.size() - 1) << " nodes) ================\n";
        out << std::left
            << std::setw(10) << "NODE"
            << std::setw(26) << "TOKEN"
            << std::setw(22) << "AST_TYPE"
            << "CT_TYPE\n";
        out << std::string(72, '-') << "\n";

        for (size_t i = 1; i < node_types.size(); ++i)
        {
            const auto& node = ast.nodes[i];
            std::string_view tok_lit = (node.start_token < ast.tokens.size())
                ? ast.tokens[node.start_token].literal
                : std::string_view{};

            std::string node_str = std::format("Node[{:02}]", i);
            std::string tok_str  = std::format("Tok[{:02}]: '{}'", node.start_token, tok_lit);
            std::string ast_str  = "[" + std::string(NodeTypeName(node.type)) + "]";
            std::string_view ct_str = (node_types[i] == CTType::Unknown())
                ? std::string_view("<none>")
                : Utils::TypeToString(node_types[i]);

            out << std::left
                << std::setw(10) << node_str
                << std::setw(26) << tok_str << (tok_str.size() >= 26 ? " " : "")
                << std::setw(22) << ast_str
                << ct_str << "\n";
        }
        out << std::string(72, '=') << "\n";
        return out.str();
    }

}
