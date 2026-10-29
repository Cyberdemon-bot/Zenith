#include <charconv>
#include "Zenith/Compiler/Parser.h"

namespace Zenith {

    Parser::Parser(std::string_view source)
         : m_Source(source), m_Lexer(source)
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
        DepthGuard guard(m_Depth);
        if (m_Depth > MAX_DEPTH) return add_error("Nesting is too deep");
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
        if (is_block_like_start(get_curr_token().type)) return parse_block_like_expr();
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
            case TokenType::ASTERISK:
            {
                uint32_t op_tok = curr_token();
                next_token();
                uint32_t expr = parse_expr(PrecedenceType::P_UNARY);
                if (expr == ERROR_NODE) return ERROR_NODE;
                uint32_t end = get_node(expr).end_token;
                uint32_t node = push_node(NodeType::DerefExpr, op_tok, end, { .child = expr });
                return node;
            }
            case TokenType::LPAREN: return parse_paren_expr();
            case TokenType::IDENT:
            {
                if (peek_token_is(TokenType::LBRACE) && !m_NoStructLit && is_struct_init_ahead())
                    return parse_struct_init();
                return parse_identifier();
            }
            case TokenType::AMPERSAND: return parse_borrow_expr(curr_token(), 0);
            case TokenType::MUT: return parse_mut_expr();
            case TokenType::NEW: return parse_new_expr(curr_token(), 0);
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
            case TokenType::EE:
            case TokenType::NE:
            case TokenType::LT:
            case TokenType::GT:
            case TokenType::LTE:
            case TokenType::GTE:
            case TokenType::AND:
            case TokenType::OR: return parse_numeric_expr(left);
            case TokenType::DOT: return parse_field_access_expr(left);
            case TokenType::LPAREN:  return parse_call_expr(left);
            case TokenType::LBRACKET: return parse_index_expr(left);
            default: return add_error("Unexpected token in binary/infix expression", left);
        }
    }

    uint32_t Parser::parse_block_like_expr()
    {
        switch (get_curr_token().type)
        {
            case TokenType::LBRACE: return parse_block_expr();
            case TokenType::IF:     return parse_if_expr();
            case TokenType::FOR:    return parse_for_expr();
            case TokenType::LABEL:  return parse_labeled_expr();
            default:                return ERROR_NODE; 
        }
    }

    uint32_t Parser::parse_labeled_expr()
    {
        uint32_t start = curr_token(), end; next_token();
        if (!curr_token_is(TokenType::COLON)) 
            return add_error("Expected ':' after label");
        next_token();

        if (curr_token_is(TokenType::LABEL) || !is_block_like_start(get_curr_token().type))
            return add_error("A label must be followed by a block, 'if', or 'for'");
        uint32_t body = parse_block_like_expr();
        if (body == ERROR_NODE) return ERROR_NODE;

        end = get_node(body).end_token;
        uint32_t label = push_node(NodeType::Label, start);
        return push_node(NodeType::LabeledExpr, start, end,
            { .labeled = { .label = label, .body = body } });
    }

    uint32_t Parser::parse_numeric_expr(uint32_t left)
    {
        uint32_t op_tok = curr_token();
        const auto& token = get_curr_token(); next_token();
        PrecedenceType precedence = get_precedence(token.type);
        
        uint32_t right = parse_expr(precedence);
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
        StructLitScope allow(m_NoStructLit, false);
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
        StructLitScope allow(m_NoStructLit, false);
        next_token(); // consume '['
        uint32_t index_expr = parse_expr(PrecedenceType::P_LOWEST);
        if (index_expr == ERROR_NODE) return ERROR_NODE;
        if (!curr_token_is(TokenType::RBRACKET)) return add_error("Expected ']' after index expression", index_expr); 
        
        uint32_t start = get_node(left).start_token, end = curr_token();
        next_token(); // consume ']'

        return push_node(NodeType::IndexExpr, start, end,
            { .access = { .target = left, .member = index_expr } });
    }

    uint32_t Parser::parse_borrow_expr(uint32_t start, uint8_t is_mut)
    {
        next_token(); // consume '&'
        if (curr_token_is(TokenType::MUT)) return add_error("Write 'mut &x' instead of '&mut x'");

        uint32_t expr = parse_expr(PrecedenceType::P_UNARY);
        if (expr == ERROR_NODE) return ERROR_NODE;

        return push_node(NodeType::BorrowExpr, start, get_node(expr).end_token,
            { .borrow = { .operand = expr, .is_mut = is_mut } });
    }

    uint32_t Parser::parse_mut_expr()
    {
        uint32_t mut_tok = curr_token(); next_token(); // consume 'mut'
        if (curr_token_is(TokenType::AMPERSAND)) return parse_borrow_expr(mut_tok, 1);
        if (curr_token_is(TokenType::NEW))       return parse_new_expr(mut_tok, 1);
        return add_error("Expected '&' or 'new' after 'mut'");
    }

    uint32_t Parser::parse_new_expr(uint32_t start, uint8_t is_mut)
    {
        next_token(); // consume 'new'
        uint32_t type = parse_named_type();
        if (type == ERROR_NODE) return ERROR_NODE;

        uint32_t args;
        if (curr_token_is(TokenType::LPAREN))
        {
            StructLitScope allow(m_NoStructLit, false);
            args = parse_delimited_list(
                NodeType::ArgList,
                TokenType::RPAREN,
                TokenType::COMMA,
                [this]() { return parse_expr(PrecedenceType::P_LOWEST); },
                "Expected ',' or ')' in constructor argument list"
            );
        }
        else if (curr_token_is(TokenType::LBRACE))
        {
            if (m_NoStructLit)
                return add_error("Struct initializer is not allowed here; wrap the expression in parentheses");
            StructLitScope allow(m_NoStructLit, false);
            args = parse_delimited_list(
                NodeType::FieldInitList,
                TokenType::RBRACE,
                TokenType::COMMA,
                [this]() { return parse_field_init(); },
                "Expected ',' or '}' in field initializer list"
            );
        }
        else return add_error("Expected '(' or '{' after type in new expression", type);

        if (args == ERROR_NODE) return ERROR_NODE;
        return push_node(NodeType::NewExpr, start, get_node(args).end_token,
            { .new_expr = { .type = type, .args = args, .is_mut = is_mut } });
    }

    uint32_t Parser::parse_paren_expr()
    {
        StructLitScope allow(m_NoStructLit, false);

        // '()' is the unit tuple
        if (peek_token_is(TokenType::RPAREN))
        {
            return parse_delimited_list(
                NodeType::Tuple, TokenType::RPAREN, TokenType::COMMA,
                [this]() { return parse_expr(PrecedenceType::P_LOWEST); },
                "Expected ',' or ')' in tuple expression"
            );
        }

        uint32_t open = curr_token(); next_token(); // consume '('
        uint32_t first = parse_expr(PrecedenceType::P_LOWEST);
        if (first == ERROR_NODE) return ERROR_NODE;

        // '(a)' is grouping, '(a,)' and '(a, b)' are tuples
        if (curr_token_is(TokenType::COMMA))
        {
            size_t marker = get_scratch_marker();
            push_scratch(first);
            while (curr_token_is(TokenType::COMMA))
            {
                next_token(); // consume ','
                if (curr_token_is(TokenType::RPAREN)) break; // trailing comma
                uint32_t elem = parse_expr(PrecedenceType::P_LOWEST);
                if (elem == ERROR_NODE) { rollback_scratch(marker); return ERROR_NODE; }
                push_scratch(elem);
            }
            if (!curr_token_is(TokenType::RPAREN))
            {
                rollback_scratch(marker);
                return add_error("Expected ',' or ')' in tuple expression");
            }
            uint32_t close = curr_token(); next_token(); // consume ')'
            auto list = commit_list(marker);
            return push_node(NodeType::Tuple, open, close, { .list = { .start = list.start, .count = list.count } });
        }

        if (!curr_token_is(TokenType::RPAREN)) return add_error("Expected ')' after expression", first);
        uint32_t close = curr_token(); next_token(); // consume ')'
        return push_node(NodeType::ParenExpr, open, close, { .child = first });
    }

    uint32_t Parser::parse_struct_init()
    {
        uint32_t name = parse_identifier();
        StructLitScope allow(m_NoStructLit, false);

        uint32_t fields = parse_delimited_list(
            NodeType::FieldInitList,
            TokenType::RBRACE,
            TokenType::COMMA,
            [this]() { return parse_field_init(); },
            "Expected ',' or '}' in field initializer list"
        );
        if (fields == ERROR_NODE) return ERROR_NODE;

        return push_node(NodeType::StructInit, get_node(name).start_token, get_node(fields).end_token,
            { .pair = { .first = name, .second = fields } });
    }

    uint32_t Parser::parse_field_init()
    {
        if (!curr_token_is(TokenType::IDENT)) return add_error("Expected field name in initializer");
        uint32_t name = parse_identifier();
        uint32_t value = name; // shorthand 'x' means 'x: x'
        if (curr_token_is(TokenType::COLON))
        {
            next_token();
            value = parse_expr(PrecedenceType::P_LOWEST);
            if (value == ERROR_NODE) return ERROR_NODE;
        }

        return push_node(NodeType::FieldInit, get_node(name).start_token, get_node(value).end_token,
            { .pair = { .first = name, .second = value } });
    }

    uint32_t Parser::parse_arrow_body()
    {
        next_token(); // consume '=>'
        if (curr_token_is(TokenType::LBRACE) || curr_token_is(TokenType::LABEL))
            return add_error("'=>' takes a single expression; use '{ }' or '=>', not both");
        return parse_expr(PrecedenceType::P_LOWEST);
    }

    uint32_t Parser::parse_block_expr()
    {
        DepthGuard guard(m_Depth);
        if (m_Depth > MAX_DEPTH) return add_error("Nesting is too deep");
        StructLitScope allow(m_NoStructLit, false);
        uint32_t start = curr_token(), end; next_token(); // consume '{'
        size_t marker = get_scratch_marker();

        while (!curr_token_is(TokenType::RBRACE) && !curr_token_is(TokenType::END_OF_FILE))
        {
            uint32_t stmt = parse_statement();
            if (stmt == ERROR_NODE) synchronize();
            else push_scratch(stmt);
        }

        if (!curr_token_is(TokenType::RBRACE))
        {
            rollback_scratch(marker);
            return add_error("Expected '}' at end of block expression");
        }
        end = curr_token(); next_token(); // consume '}'

        auto list = commit_list(marker);
        return push_node(NodeType::BlockExpr, start, end,
            { .list = { .start = list.start, .count = list.count } });
    }

    uint32_t Parser::parse_if_expr()
    {
        DepthGuard guard(m_Depth);
        if (m_Depth > MAX_DEPTH) return add_error("Nesting is too deep");
        uint32_t start = curr_token(), end, cond; next_token(); // consume 'if'
        {
            StructLitScope restrict_literals(m_NoStructLit, true);
            cond = parse_expr(PrecedenceType::P_LOWEST);
        }
        if (cond == ERROR_NODE) return ERROR_NODE;

        uint32_t then_branch = ERROR_NODE;
        if (curr_token_is(TokenType::FATARROW)) then_branch = parse_arrow_body();
        else if (curr_token_is(TokenType::LBRACE)) then_branch = parse_block_expr();
        else return add_error("Expected '=>' or '{' after if condition", cond);

        if (then_branch == ERROR_NODE) return ERROR_NODE;

        uint32_t else_branch = ERROR_NODE;
        if (curr_token_is(TokenType::ELSE))
        {
            next_token(); // consume 'else'
            if (curr_token_is(TokenType::IF)) else_branch = parse_if_expr();
            else if (curr_token_is(TokenType::LBRACE)) else_branch = parse_block_expr();
            else if (curr_token_is(TokenType::FATARROW)) else_branch = parse_arrow_body();
            else return add_error("Expected 'if', '=>', or '{' after 'else'");

            if (else_branch == ERROR_NODE) return ERROR_NODE;
        }

        end = (else_branch != ERROR_NODE) ? get_node(else_branch).end_token : get_node(then_branch).end_token;
        return push_node(NodeType::IfExpr, start, end,
            { .if_expr = { .cond = cond, .then_branch = then_branch, .else_branch = else_branch } });
    }

    uint32_t Parser::parse_for_expr()
    {
        uint32_t start = curr_token(), end, cond; next_token(); // consume 'for'

        // form: for {}
        if (curr_token_is(TokenType::LBRACE)) // parse body
        {
            uint32_t body = parse_block_expr();
            if (body == ERROR_NODE) return ERROR_NODE;
            end = get_node(body).end_token;
            return push_node(NodeType::ForExpr, start, end,
                { .for_expr = { .cond = ERROR_NODE, .body = body } });
        }

        // form: for item in items {}
        if (curr_token_is(TokenType::IDENT) && peek_token_is(TokenType::IN))
        {
            uint32_t ident = parse_identifier(), iterable;
            next_token(); // consume 'in'

            { //parse iterable
                StructLitScope restrict_literals(m_NoStructLit, true);
                iterable = parse_expr(PrecedenceType::P_LOWEST);
            }
            if (iterable == ERROR_NODE) return ERROR_NODE;

            if (!curr_token_is(TokenType::LBRACE))
                return add_error("Expected '{' to begin body block of 'for' expression", iterable);

            uint32_t body = parse_block_expr();
            if (body == ERROR_NODE) return ERROR_NODE;
            end = get_node(body).end_token;

            return push_node(NodeType::ForInExpr, start, end,
                { .for_in = { .var = ident, .iterable = iterable, .body = body } });
        }

        // form: for cond {}
        { // parse condition
            StructLitScope restrict_literals(m_NoStructLit, true);
            cond = parse_expr(PrecedenceType::P_LOWEST);
        }
        if (cond == ERROR_NODE) return ERROR_NODE;
        if (!curr_token_is(TokenType::LBRACE))
            return add_error("Expected '{' to begin body block of 'for' expression", cond);
        uint32_t body = parse_block_expr(); // parse body
        if (body == ERROR_NODE) return ERROR_NODE;
        end = get_node(body).end_token;
        return push_node(NodeType::ForExpr, start, end,
            { .for_expr = { .cond = cond, .body = body } });
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
        bool block_like = is_block_like_start(get_curr_token().type);
        uint32_t expr = block_like ? parse_block_like_expr() : parse_expr(PrecedenceType::P_LOWEST);
        if (expr == ERROR_NODE) return ERROR_NODE;
        if (!block_like && curr_token_is(TokenType::EQ)) return parse_assign_stmt(expr);

        if (!block_like && m_Nodes[expr].type != NodeType::CallExpr)
            return add_error("Expression is not valid as a statement; only calls can be used as statements", expr, ErrorScope::NodeStart);

        uint32_t start = get_node(expr).start_token, end = get_node(expr).end_token;
        bool closed_by_brace = block_like && m_Tokens[end].type == TokenType::RBRACE;

        if (curr_token_is(TokenType::SEMICOLON)) { end = curr_token(); next_token(); }
        else if (!closed_by_brace)
            return add_error("Expected ';' after expression statement", expr, ErrorScope::TokenBefore);

        return push_node(NodeType::ExprStmt, start, end, { .child = expr });
    }

    uint32_t Parser::parse_assign_stmt(uint32_t lhs)
    {
        NodeType lt = m_Nodes[lhs].type;
        if (lt != NodeType::Identifier &&
            lt != NodeType::FieldAccess &&
            lt != NodeType::IndexExpr   &&
            lt != NodeType::DerefExpr)
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

        if (!curr_token_is(TokenType::LABEL))
            return add_error("Expected a label after 'break'; unlabeled break is not allowed");
        label = push_node(NodeType::Label, curr_token());
        next_token();

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

        uint32_t params = parse_delimited_list(
            NodeType::ArgList,
            TokenType::RPAREN,
            TokenType::COMMA,
            [this]() { return parse_typed_ident("Expected parameter name"); },
            "Expected ',' or ')' in parameter list"
        );
        if (params == ERROR_NODE) return ERROR_NODE;
        uint32_t rparen = get_node(params).end_token;

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
            { .func_sign = { .name = name, .params = params, .ret_type = ret_type } });

        if (!curr_token_is(TokenType::LBRACE))
            return add_error("Expected '{' for function body", sign);

        uint32_t body = parse_block_expr();
        if (body == ERROR_NODE) return ERROR_NODE;

        uint32_t end = get_node(body).end_token;
        return push_node(NodeType::FuncDecl, start, end,
            { .func_decl = { .sign = sign, .body = body } });
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

        uint32_t fields = parse_delimited_list(
            NodeType::ArgList,
            TokenType::RBRACE,
            TokenType::COMMA,
            [this]() { return parse_typed_ident("Expected field name in struct body"); },
            "Expected ',' or '}' in struct body"
        );
        if (fields == ERROR_NODE) return ERROR_NODE;

        return push_node(NodeType::StructDecl, start, get_node(fields).end_token,
            { .struct_decl = { .name = name, .fields = fields } });
    }

    uint32_t Parser::parse_typed_ident(std::string_view missing_name_message)
    {
        if (!curr_token_is(TokenType::IDENT)) return add_error(missing_name_message);
        uint32_t ident = parse_identifier();

        if (!curr_token_is(TokenType::COLON)) return add_error("Expected ':' after name", ident);
        next_token();

        uint32_t type = parse_type_annotation();
        if (type == ERROR_NODE) return ERROR_NODE;

        return push_node(NodeType::TypedIdentifier,
            get_node(ident).start_token, get_node(type).end_token,
            { .typed_ident = { .ident = ident, .type = type, .is_mut = 0 } });
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
        DepthGuard guard(m_Depth);
        if (m_Depth > MAX_DEPTH) return add_error("Nesting is too deep");
        uint32_t start = curr_token();

        switch (get_curr_token().type)
        {
            case TokenType::MUT:
            {
                next_token();
                if (!curr_token_is(TokenType::AMPERSAND))
                    return add_error("Expected '&' after 'mut' in type; 'mut' only applies to references");
                return parse_ref_type(start, 1);
            }
            case TokenType::AMPERSAND:
            {
                if (peek_token_is(TokenType::MUT))
                {
                    next_token();
                    return add_error("Write 'mut &T' instead of '&mut T'");
                }
                return parse_ref_type(start, 0);
            }
            case TokenType::LPAREN: return parse_tuple_type();
            case TokenType::DYN:    return parse_dyn_type(); 
            case TokenType::IDENT:  return parse_named_type();
            default: return add_error("Expected type annotation");
        }
    }

    uint32_t Parser::parse_ref_type(uint32_t start, uint8_t is_mut)
    {
        next_token(); // consume '&'
        uint32_t inner = parse_type_annotation();
        if (inner == ERROR_NODE) return ERROR_NODE;

        return push_node(NodeType::RefType, start, get_node(inner).end_token,
            { .ref_type = { .inner = inner, .is_mut = is_mut } });
    }

    uint32_t Parser::parse_named_type()
    {
        if (!curr_token_is(TokenType::IDENT)) return add_error("Expected type identifier in type annotation");

        uint32_t ident_tok = curr_token(), end = ident_tok;
        next_token();

        uint32_t args = ERROR_NODE;
        if (curr_token_is(TokenType::LT))
        {
            args = parse_delimited_list(
                NodeType::TypeArgList,
                TokenType::GT,
                TokenType::COMMA,
                [this]() { return parse_type_annotation(); },
                "Expected ',' or '>' in type arguments"
            );
            if (args == ERROR_NODE) return ERROR_NODE;
            end = get_node(args).end_token;
        }

        return push_node(NodeType::NamedType, ident_tok, end,
            { .named_type = { .ident = ident_tok, .args = args } });
    }

    uint32_t Parser::parse_dyn_type()
    {
        uint32_t start = curr_token();
        next_token(); // consume 'dyn'

        uint32_t inner = parse_named_type();
        if (inner == ERROR_NODE)
            return add_error("Expected trait/class name after 'dyn'");

        return push_node(NodeType::DynType, start, get_node(inner).end_token,
            { .child = inner });
    }

    uint32_t Parser::parse_tuple_type()
    {
        return parse_delimited_list(
            NodeType::TupleType,
            TokenType::RPAREN,
            TokenType::COMMA,
            [this]() { return parse_type_annotation(); },
            "Expected ',' or ')' in tuple type"
        );
    }

    // other
    bool Parser::is_struct_init_ahead() const
    {
        TokenType first = peek_type(2);
        if (first == TokenType::RBRACE) return true;
        if (first != TokenType::IDENT) return false;

        TokenType second = peek_type(3);
        return second == TokenType::COLON || second == TokenType::COMMA || second == TokenType::RBRACE;
    }

    void Parser::synchronize()
    {
        uint32_t depth = 0;
        while (!curr_token_is(TokenType::END_OF_FILE))
        {
            TokenType t = get_curr_token().type;
            if (t == TokenType::LBRACE) ++depth;
            else if (t == TokenType::RBRACE)
            {
                if (depth == 0) return; 
                --depth;
            }
            else if (depth == 0)
            {
                if (t == TokenType::SEMICOLON) { next_token(); return; }
                if (t == TokenType::LET      || t == TokenType::RETURN || t == TokenType::BREAK ||
                    t == TokenType::CONTINUE || t == TokenType::FUNC   || t == TokenType::STRUCT ||
                    t == TokenType::PUB) return;
            }
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