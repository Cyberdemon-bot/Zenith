#include <charconv>
#include "Zenith/Compiler/Parser.h"

namespace Zenith {

    Parser::Parser(std::string_view source)
         : m_Lexer(source), m_Source(source)
    {
        m_Tokens.reserve(256);
        m_Nodes.push_back({});
        Token token;
        do 
        {
            token = m_Lexer.next_token();
            m_Tokens.push_back(token);
        } while (token.type != TokenType::END_OF_FILE);
    }

    uint32_t Parser::parse_program()
    {
        size_t marker = get_scratch_marker();
        uint32_t start = curr_token(), end;

        while (!curr_token_is(TokenType::END_OF_FILE))
        {
            if (curr_token_is(TokenType::RBRACE))
            {
                add_error("Unexpected '}' at top level");
                next_token();
                continue;
            }

            uint32_t stmt = parse_statement();
            if (stmt == ERROR_NODE) synchronize();
            else push_scratch(stmt);
        }

        end = curr_token();
        auto list = commit_list(marker);
        return push_node(NodeType::Program, start, end, { .list = { .start = list.start, .count = list.count } });
    }

    // expression parsing
    uint32_t Parser::parse_expr(PrecedenceType precedence)
    {
        uint32_t left = parse_prefix_expr();
        if (left == ERROR_NODE) return ERROR_NODE;
        while (precedence < get_precedence(get_curr_token().type))
        {
            left = parse_infix_expr(left);
            if (left == ERROR_NODE) return ERROR_NODE;
        }
        return left;
    }

    uint32_t Parser::parse_prefix_expr()
    {
        const auto& curr = get_curr_token();
        switch (curr.type) 
        {
            case TokenType::INTEGER: 
            {
                Token tok = get_curr_token();
                int64_t val = 0;
                auto [ptr, ec] = std::from_chars(tok.literal.data(), tok.literal.data() + tok.literal.size(), val);
                if (ec != std::errc{}) return add_error("Integer literal out of range");
                uint32_t idx = push_node(NodeType::Integer, curr_token(), { .i64 = val });
                next_token(); 
                return idx;
            }
            case TokenType::DECIMAL: 
            {
                Token tok = get_curr_token();
                double val = 0.0;
                auto [ptr, ec] = std::from_chars(tok.literal.data(), tok.literal.data() + tok.literal.size(), val);
                if (ec != std::errc{}) return add_error("Invalid decimal literal");
                uint32_t idx = push_node(NodeType::Decimal, curr_token(), { .f64 = val });
                next_token(); 
                return idx;
            }
            case TokenType::STRING: 
            {
                uint32_t idx = push_node(NodeType::String, curr_token()); 
                next_token(); 
                return idx;
            }
            case TokenType::TRUE:
            {
                uint32_t idx = push_node(NodeType::Bool, curr_token(), { .i64 = 1 });
                next_token();
                return idx;
            }
            case TokenType::FALSE:
            {
                uint32_t idx = push_node(NodeType::Bool, curr_token(), { .i64 = 0 });
                next_token();
                return idx;
            }
            case TokenType::NULL_LIT:
            {
                uint32_t idx = push_node(NodeType::Null, curr_token());
                next_token();
                return idx;
            }
            case TokenType::BANG:
            case TokenType::MINUS: 
            {
                uint32_t op_tok = curr_token(); next_token(); 
                uint32_t expr = parse_expr(PrecedenceType::P_UNARY);
                if (expr == ERROR_NODE) return ERROR_NODE;
                return push_node(NodeType::UnaryExpr, op_tok, get_node(expr).end_token,
                    { .unary = { .op = op_tok, .operand = expr } });
            }
            case TokenType::LPAREN:
            {
                next_token(); 
                uint32_t expr = parse_expr(PrecedenceType::P_LOWEST);
                if (expr == ERROR_NODE) return ERROR_NODE;
                if (!curr_token_is(TokenType::RPAREN)) return add_error("Expected ')' after expression", expr);
                next_token(); 
                return expr;
            }
            case TokenType::LABEL:
            {
                uint32_t start = curr_token(), end; next_token();
                if (!curr_token_is(TokenType::COLON)) return add_error("Expected ':' after label");

                next_token();
                if (!curr_token_is(TokenType::LBRACE)) 
                    return add_error("Label definition required a block expression");
                uint32_t expr = parse_block_expr();
                if (expr == ERROR_NODE) return ERROR_NODE;
                end = get_node(expr).end_token;

                uint32_t label = push_node(NodeType::Label, start);
                return push_node(NodeType::LabeledBlockExpr, start, end,
                    { .labeled = { .label = label, .body = expr } });
            }
            case TokenType::IDENT:     return parse_identifier();
            case TokenType::AMPERSAND: return parse_borrow_expr();
            case TokenType::LBRACE:    return parse_block_expr();
            case TokenType::IF:        return parse_if_expr();
            default: return add_error("Unexpected token in prefix expression");
        }
    }

    uint32_t Parser::parse_infix_expr(uint32_t left)
    {
        const auto& op_token = get_curr_token();

        switch (op_token.type)
        {
            case TokenType::PLUS:
            case TokenType::MINUS:
            case TokenType::ASTERISK:
            case TokenType::SLASH:
            case TokenType::MODULUS:
            case TokenType::POW:
            case TokenType::EE:
            case TokenType::NE:
            case TokenType::LT:
            case TokenType::GT:
            case TokenType::LTE:
            case TokenType::GTE: return parse_numeric_expr(left);
            case TokenType::DOT: return parse_field_access_expr(left);
            case TokenType::LPAREN:  return parse_call_expr(left);
            case TokenType::LBRACKET: return parse_index_expr(left);
            default: return add_error("Unexpected token in binary/infix expression", left);
        }
    }

    uint32_t Parser::parse_numeric_expr(uint32_t left)
    {
        uint32_t op_tok = curr_token();
        const auto& token = get_curr_token(); next_token();
        PrecedenceType precedence = get_precedence(token.type), right_precedence = precedence;
        
        if (token.type == TokenType::POW) 
            right_precedence = static_cast<PrecedenceType>(static_cast<uint8_t>(precedence) - 1);
        uint32_t right = parse_expr(right_precedence);
        if (right == ERROR_NODE) return ERROR_NODE;

        uint32_t start = get_node(left).start_token, end = get_node(right).end_token;
        return push_node(NodeType::BinaryExpr, start, end,
            { .binary = { .lhs = left, .op = op_tok, .rhs = right } });
    }

    uint32_t Parser::parse_field_access_expr(uint32_t left)
    {
        next_token(); // consume '.'
        if (!curr_token_is(TokenType::IDENT)) return add_error("Expected field identifier after '.'", left);
        uint32_t field = parse_identifier();
        uint32_t start = get_node(left).start_token, end = get_node(field).end_token;
        return push_node(NodeType::FieldAccess, start, end,
            { .access = { .target = left, .member = field } });
    }

    uint32_t Parser::parse_call_expr(uint32_t left)
    {
        uint32_t args = parse_delimited_list(
            NodeType::ArgList,
            TokenType::RPAREN,
            TokenType::COMMA,
            [this]() { return parse_expr(PrecedenceType::P_LOWEST); },
            "Expected ',' or ')' in function call argument list"
        );
        if (args == ERROR_NODE) return ERROR_NODE;

        uint32_t start = get_node(left).start_token, end = get_node(args).end_token;
        return push_node(NodeType::CallExpr, start, end,
            { .call = { .callee = left, .args = args } });
    }

    uint32_t Parser::parse_index_expr(uint32_t left)
    {
        next_token(); // consume '['
        uint32_t index_expr = parse_expr(PrecedenceType::P_LOWEST);
        if (index_expr == ERROR_NODE) return ERROR_NODE;
        if (!curr_token_is(TokenType::RBRACKET)) return add_error("Expected ']' after index expression", index_expr); 
        
        uint32_t start = get_node(left).start_token, end = curr_token();
        next_token(); // consume ']'

        return push_node(NodeType::IndexExpr, start, end,
            { .access = { .target = left, .member = index_expr } });
    }

    uint32_t Parser::parse_borrow_expr()
    {
        uint32_t amp_tok = curr_token(); next_token(); // consume '&'
        uint8_t is_mut = 0;
        if (curr_token_is(TokenType::MUT)) { is_mut = 1; next_token(); }

        uint32_t expr = parse_expr(PrecedenceType::P_UNARY);
        if (expr == ERROR_NODE) return ERROR_NODE;

        return push_node(NodeType::BorrowExpr, amp_tok, get_node(expr).end_token,
            { .borrow = { .operand = expr, .is_mut = is_mut } });
    }

    uint32_t Parser::parse_block_expr()
    {
        uint32_t start = curr_token(), end; next_token(); // consume '{'
        size_t marker = get_scratch_marker();

        while (!curr_token_is(TokenType::RBRACE) && !curr_token_is(TokenType::END_OF_FILE))
        {
            uint32_t stmt = parse_statement();
            if (stmt == ERROR_NODE) synchronize();
            else push_scratch(stmt);
        }

        if (!curr_token_is(TokenType::RBRACE)) return add_error("Expected '}' at end of block expression");
        end = curr_token(); next_token(); // consume '}'

        auto list = commit_list(marker);
        return push_node(NodeType::BlockExpr, start, end,
            { .list = { .start = list.start, .count = list.count } });
    }

    uint32_t Parser::parse_if_expr()
    {
        uint32_t if_tok = curr_token(); next_token(); // consume 'if'

        uint32_t cond = parse_expr(PrecedenceType::P_LOWEST);
        if (cond == ERROR_NODE) return ERROR_NODE;

        uint32_t then_branch = ERROR_NODE;
        if (curr_token_is(TokenType::FATARROW))
        {
            next_token(); // consume '=>'
            then_branch = parse_expr(PrecedenceType::P_LOWEST);
        }
        else if (curr_token_is(TokenType::LBRACE)) then_branch = parse_block_expr();
        else return add_error("Expected '=>' or '{' after if condition", cond);

        if (then_branch == ERROR_NODE) return ERROR_NODE;

        uint32_t else_branch = ERROR_NODE;
        if (curr_token_is(TokenType::ELSE))
        {
            next_token(); // consume 'else'
            if (curr_token_is(TokenType::IF)) else_branch = parse_if_expr();
            else if (curr_token_is(TokenType::LBRACE)) else_branch = parse_block_expr();
            else if (curr_token_is(TokenType::FATARROW))
            {
                next_token(); // consume '=>'
                else_branch = parse_expr(PrecedenceType::P_LOWEST);
            }
            else return add_error("Expected 'if', '=>', or '{' after 'else'");

            if (else_branch == ERROR_NODE) return ERROR_NODE;
        }

        uint32_t end = (else_branch != ERROR_NODE) ? get_node(else_branch).end_token : get_node(then_branch).end_token;

        return push_node(NodeType::IfExpr, if_tok, end,
            { .if_expr = { .cond = cond, .then_branch = then_branch, .else_branch = else_branch } });
    }

    // statements
    uint32_t Parser::parse_statement()
    {
        while (curr_token_is(TokenType::SEMICOLON)) next_token(); // consume all leftover ';'
        if (curr_token_is(TokenType::END_OF_FILE) || curr_token_is(TokenType::RBRACE)) return ERROR_NODE;

        switch (get_curr_token().type)
        {
            case TokenType::LET: return parse_let_stmt();
            case TokenType::RETURN: return parse_return_stmt();
            case TokenType::BREAK: return parse_break_stmt();
            case TokenType::CONTINUE: return parse_continue_stmt();
            case TokenType::FUNC: return parse_func_decl(curr_token());
            case TokenType::STRUCT: return parse_struct_decl(curr_token());
            case TokenType::PUB:
            {
                uint32_t start = curr_token();
                next_token();
                if (curr_token_is(TokenType::FUNC))   return parse_func_decl(start);
                if (curr_token_is(TokenType::STRUCT)) return parse_struct_decl(start);
                return add_error("Expected 'func' or 'struct' after 'pub'", start);
            }
            default: return default_parser();
        }
    }

    uint32_t Parser::default_parser()
    {
        uint32_t expr = parse_expr(PrecedenceType::P_LOWEST);
        if (expr == ERROR_NODE) return ERROR_NODE;
        if (curr_token_is(TokenType::EQ)) return parse_assign_stmt(expr);

        NodeType t = m_Nodes[expr].type;
        bool is_expr_as_stmt_node =
            t == NodeType::BlockExpr        ||
            t == NodeType::LabeledBlockExpr ||
            t == NodeType::IfExpr           ||
            t == NodeType::CallExpr;

        if (!is_expr_as_stmt_node)
            return add_error("Expression is not valid as a statement; expected call, block, if, or assignment", expr, ErrorScope::NodeStart);

        uint32_t start = get_node(expr).start_token, end = get_node(expr).end_token;

        if (curr_token_is(TokenType::SEMICOLON)) { end = curr_token(); next_token(); }
        else if (!curr_token_is(TokenType::RBRACE) && !curr_token_is(TokenType::END_OF_FILE))
            return add_error("Expected ';' after expression statement", expr, ErrorScope::TokenBefore);

        return push_node(NodeType::ExprStmt, start, end, { .child = expr });
    }

    uint32_t Parser::parse_assign_stmt(uint32_t lhs)
    {
        NodeType lt = m_Nodes[lhs].type;
        if (lt != NodeType::Identifier &&
            lt != NodeType::FieldAccess &&
            lt != NodeType::IndexExpr)
            return add_error("Invalid assignment target", lhs, ErrorScope::NodeStart);

        uint32_t op_tok = curr_token(); next_token();
        uint32_t rhs = parse_expr(PrecedenceType::P_LOWEST);
        if (rhs == ERROR_NODE) return ERROR_NODE;
        if (!curr_token_is(TokenType::SEMICOLON))
            return add_error("Expected ';' after assignment statement", rhs, ErrorScope::TokenBefore);

        uint32_t start = get_node(lhs).start_token, end = curr_token(); next_token();
        return push_node(NodeType::AssignStmt, start, end,
            { .binary = { .lhs = lhs, .op = op_tok, .rhs = rhs } });
    }

    uint32_t Parser::parse_let_stmt()
    {
        uint32_t start = curr_token(), end; next_token();

        uint8_t is_mut = 0;
        if (curr_token_is(TokenType::MUT)) { is_mut = 1; next_token(); }

        if (!curr_token_is(TokenType::IDENT)) return add_error("Expected identifier after 'let'");
        uint32_t ident = parse_identifier();

        uint32_t type_annot = ERROR_NODE;
        if (curr_token_is(TokenType::COLON))
        {
            next_token();
            if (!curr_token_is(TokenType::AMPERSAND) && 
                !curr_token_is(TokenType::MUT) && 
                !curr_token_is(TokenType::IDENT))
                return add_error("Expected type annotation after ':'", ident);

            type_annot = parse_type_annotation();
            if (type_annot == ERROR_NODE) return ERROR_NODE;
        }

        uint32_t target;
        if (type_annot == ERROR_NODE && is_mut == 0) target = ident;
        else
        {
            uint32_t t_start = get_node(ident).start_token;
            uint32_t t_end = (type_annot != ERROR_NODE) ? get_node(type_annot).end_token : get_node(ident).end_token;
            target = push_node(NodeType::TypedIdentifier, t_start, t_end,
                { .typed_ident = { .ident = ident, .type = type_annot, .is_mut = is_mut } });
        }

        if (!curr_token_is(TokenType::EQ)) 
            return add_error("Expected '=' in variable declaration", target);
        next_token(); // consume '='

        uint32_t value = parse_expr(PrecedenceType::P_LOWEST);
        if (value == ERROR_NODE) return ERROR_NODE;

        if (!curr_token_is(TokenType::SEMICOLON)) 
            return add_error("Expected ';' after let statement", value, ErrorScope::TokenBefore);
        end = curr_token(); next_token();
        return push_node(NodeType::LetStmt, start, end,
            { .let = { .target = target, .value = value } });
    }
    
    uint32_t Parser::parse_return_stmt()
    {
        uint32_t ret_tok = curr_token(), end; next_token();
        uint32_t expr = ERROR_NODE;

        if (!curr_token_is(TokenType::SEMICOLON))
        {
            expr = parse_expr(PrecedenceType::P_LOWEST);
            if (expr == ERROR_NODE) return ERROR_NODE;
        }

        if (!curr_token_is(TokenType::SEMICOLON))
            return add_error("Expected ';' after return statement", expr, ErrorScope::TokenBefore);
        end = curr_token(); next_token();

        return push_node(NodeType::ReturnStmt, ret_tok, end, { .child = expr });
    }

    uint32_t Parser::parse_break_stmt()
    {   
        uint32_t break_tok = curr_token(), end; next_token();
        uint32_t label = ERROR_NODE, value = ERROR_NODE;

        if (curr_token_is(TokenType::SEMICOLON))
        {
            uint32_t end = curr_token(); next_token();
            return push_node(NodeType::BreakStmt, break_tok, end,
                { .break_stmt = { .label = ERROR_NODE, .value = ERROR_NODE } });
        }

        if (curr_token_is(TokenType::LABEL))
        {
            label = push_node(NodeType::Label, curr_token());
            next_token();
        }

        if (!curr_token_is(TokenType::SEMICOLON))
        {
            value = parse_expr(PrecedenceType::P_LOWEST);
            if (value == ERROR_NODE) return ERROR_NODE;
        }

        if (!curr_token_is(TokenType::SEMICOLON)) 
        {
            uint32_t ref = (value != ERROR_NODE) ? value : label;
            return add_error("Expected ';' after break statement", ref, ErrorScope::TokenBefore);
        }
        end = curr_token(); next_token();

        return push_node(NodeType::BreakStmt, break_tok, end,
            { .break_stmt = { .label = label, .value = value } });
    }   

    uint32_t Parser::parse_continue_stmt()
    {
        uint32_t cont_tok = curr_token(), end; next_token();

        if (!curr_token_is(TokenType::SEMICOLON)) 
            return add_error("Expected ';' after continue statement", ERROR_NODE, ErrorScope::TokenBefore);
        end = curr_token(); next_token();
        return push_node(NodeType::ContinueStmt, cont_tok, end);
    }

    // declarations
    uint32_t Parser::parse_func_decl(uint32_t start)
    {
        next_token(); // consume 'func'
        if (!curr_token_is(TokenType::IDENT)) return add_error("Expected function name after 'func'");
        uint32_t name = parse_identifier();

        if (!curr_token_is(TokenType::LPAREN)) return add_error("Expected '(' after function name", name);
        uint32_t lparen = curr_token(); next_token(); // consume '('
        size_t marker = get_scratch_marker();

        if (!curr_token_is(TokenType::RPAREN))
        {
            while (true)
            {
                uint32_t arg = parse_func_arg_decl();
                if (arg == ERROR_NODE) { rollback_scratch(marker); return ERROR_NODE; }
                push_scratch(arg);

                if (curr_token_is(TokenType::COMMA)) { next_token(); continue; }
                if (curr_token_is(TokenType::RPAREN)) break;

                rollback_scratch(marker);
                return add_error("Expected ',' or ')' in parameter list");
            }
        }

        if (!curr_token_is(TokenType::RPAREN))
        {
            rollback_scratch(marker);
            return add_error("Expected ')' after parameter list");
        }
        uint32_t rparen = curr_token(); next_token(); // consume ')'

        auto params = commit_list(marker);
        uint32_t params_node = push_node(NodeType::ArgList, lparen, rparen,
            { .list = { .start = params.start, .count = params.count } });

        uint32_t ret_type = ERROR_NODE;
        if (curr_token_is(TokenType::ARROW))
        {
            next_token(); // consume '->'
            ret_type = parse_type_annotation();
            if (ret_type == ERROR_NODE) return ERROR_NODE;
        }

        uint32_t sign_end = (ret_type != ERROR_NODE) ? get_node(ret_type).end_token : rparen;
        uint32_t sign = push_node(NodeType::FuncSign,
            get_node(name).start_token, sign_end,
            { .func_sign = { .name = name, .params = params_node, .ret_type = ret_type } });

        if (!curr_token_is(TokenType::LBRACE))
            return add_error("Expected '{' for function body", sign);

        uint32_t body = parse_block_expr();
        if (body == ERROR_NODE) return ERROR_NODE;

        uint32_t end = get_node(body).end_token;
        return push_node(NodeType::FuncDecl, start, end,
            { .func_decl = { .sign = sign, .body = body } });
    }

    uint32_t Parser::parse_func_arg_decl()
    {
        if (!curr_token_is(TokenType::IDENT)) return add_error("Expected parameter name");
        uint32_t ident = parse_identifier();

        if (!curr_token_is(TokenType::COLON)) return add_error("Expected ':' after parameter name", ident);
        next_token();

        uint32_t type = parse_type_annotation();
        if (type == ERROR_NODE) return ERROR_NODE;

        uint32_t start = get_node(ident).start_token, end = get_node(type).end_token;
        return push_node(NodeType::FuncArgDecl, start, end,
            { .func_arg = { .ident = ident, .type = type, .default_val = ERROR_NODE } });
    }

    uint32_t Parser::parse_struct_decl(uint32_t start)
    {
        next_token(); // consume 'struct'

        if (!curr_token_is(TokenType::IDENT)) return add_error("Expected struct name after 'struct'");
        uint32_t name = parse_identifier();

        // unit form
        if (curr_token_is(TokenType::SEMICOLON))
        {
            uint32_t end = curr_token(); next_token();
            return push_node(NodeType::StructDecl, start, end,
                { .struct_decl = { .name = name, .fields = ERROR_NODE } });
        }

        if (!curr_token_is(TokenType::LBRACE))
            return add_error("Expected '{' or ';' after struct name", name);

        uint32_t lbrace = curr_token(); next_token(); // consume '{'
        size_t marker = get_scratch_marker();

        while (!curr_token_is(TokenType::RBRACE) && !curr_token_is(TokenType::END_OF_FILE))
        {
            if (!curr_token_is(TokenType::IDENT))
            {
                rollback_scratch(marker);
                return add_error("Expected field name in struct body");
            }
            uint32_t field_ident = parse_identifier();

            if (!curr_token_is(TokenType::COLON))
            {
                rollback_scratch(marker);
                return add_error("Expected ':' after field name", field_ident);
            }
            next_token();

            uint32_t field_type = parse_type_annotation();
            if (field_type == ERROR_NODE) { rollback_scratch(marker); return ERROR_NODE; }

            uint32_t field = push_node(NodeType::TypedIdentifier,
                get_node(field_ident).start_token, get_node(field_type).end_token,
                { .typed_ident = { .ident = field_ident, .type = field_type, .is_mut = 0 } });
            push_scratch(field);

            if (curr_token_is(TokenType::COMMA)) { next_token(); continue; }
            if (curr_token_is(TokenType::RBRACE)) break;

            rollback_scratch(marker);
            return add_error("Expected ',' or '}' in struct body");
        }

        if (!curr_token_is(TokenType::RBRACE))
        {
            rollback_scratch(marker);
            return add_error("Expected '}' at end of struct");
        }
        uint32_t end = curr_token(); next_token();

        auto fields = commit_list(marker);
        uint32_t fields_node = push_node(NodeType::ArgList, lbrace, end,
            { .list = { .start = fields.start, .count = fields.count } });

        return push_node(NodeType::StructDecl, start, end,
            { .struct_decl = { .name = name, .fields = fields_node } });
    }

    // helpers
    uint32_t Parser::parse_identifier()
    {
        uint32_t node = push_node(NodeType::Identifier, curr_token());
        next_token(); 
        return node;
    }

    uint32_t Parser::parse_type_annotation()
    {
        uint32_t start = curr_token(), end;
        uint8_t flags = 0;

        if (curr_token_is(TokenType::AMPERSAND)) { flags |= TYPE_FLAG_BORROW; next_token(); }
        if (curr_token_is(TokenType::MUT)) { flags |= TYPE_FLAG_MUT;    next_token(); }
        if (!curr_token_is(TokenType::IDENT)) return add_error("Expected type identifier in type annotation");

        uint32_t ident_tok = curr_token();  // record type name
        end = curr_token();
        next_token();

        return push_node(NodeType::TypeAnnotation, start, end,
            { .type_annot = { .ident = ident_tok, .flags = flags } });
    }

    // other
    void Parser::synchronize()
    {
        while (!curr_token_is(TokenType::END_OF_FILE))
        {
            if (curr_token_is(TokenType::SEMICOLON)) { next_token(); return; }
            if (curr_token_is(TokenType::RBRACE)) return;

            if (curr_token_is(TokenType::LET)      ||
                curr_token_is(TokenType::RETURN)   ||
                curr_token_is(TokenType::BREAK)    ||
                curr_token_is(TokenType::CONTINUE) ||
                curr_token_is(TokenType::FUNC)     ||
                curr_token_is(TokenType::STRUCT)   ||
                curr_token_is(TokenType::PUB)) return;

            next_token();
        }
    }

    uint32_t Parser::add_error(std::string_view message, uint32_t node, ErrorScope scope)
    {
        uint32_t pos = curr_token();

        switch (scope)
        {
            case ErrorScope::NodeStart:
                if (node != ERROR_NODE && node < m_Nodes.size()) pos = m_Nodes[node].start_token;
                break;
            case ErrorScope::TokenBefore:
                if (m_Cursor > 0) pos = m_Cursor - 1;
                break;
            case ErrorScope::Token:
            default: break;
        }
        if (pos >= m_Tokens.size()) pos = static_cast<uint32_t>(m_Tokens.size()) - 1;

        const Token& tok = m_Tokens[pos];
        m_Errors.push_back(ParseError
        {
            .message = std::string(message),
            .node    = node,
            .token   = pos,
            .scope   = scope
        });
        return ERROR_NODE;
    }

    ListHeader Parser::commit_list(size_t marker)
    {
        uint32_t count = static_cast<uint32_t>(m_Scratch.size() - marker);
        uint32_t start = static_cast<uint32_t>(m_Extra.size());
        m_Extra.insert(m_Extra.end(), m_Scratch.begin() + marker, m_Scratch.end());
        m_Scratch.resize(marker);
        return { .start = start, .count = count };
    }
}