#include <charconv>
#include <format>
#include "Zenith/Compiler/Parser.h"

namespace Zenith {

    Parser::Parser(std::string_view source)
        : m_Lexer(source)
    {
        m_Tokens.reserve(256);
        m_Nodes.push_back({}); // ERROR_NODE
        Token token;
        do 
        {
            token = m_Lexer.next_token();
            m_Tokens.push_back(token);
        } while (token.type != TokenType::END_OF_FILE);
    }

    uint32_t Parser::parse_program()
    {
        uint32_t start = get_list_start();
        uint32_t count = 0;
        uint32_t first_tok = curr_token();

        while (!curr_token_is(TokenType::END_OF_FILE))
        {
            uint32_t stmt = parse_statement();
            if (stmt == ERROR_NODE) synchronize(); 
            else push_list(stmt, count);
        }

        return push_node(NodeType::Program, first_tok, { .list = { .start = start, .count = count } });
    }

    // parse expr
    uint32_t Parser::parse_expr(PrecedenceType precedence)
    {
        uint32_t left = parse_prefix_expr();
        if (left == ERROR_NODE) return ERROR_NODE;
        while (!curr_token_is(TokenType::SEMICOLON) && 
               !curr_token_is(TokenType::END_OF_FILE) &&
               precedence < get_precedence(get_curr_token().type))
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
                Token tok = get_curr_token(); int64_t val = 0;
                std::from_chars(tok.literal.data(), tok.literal.data() + tok.literal.size(), val);
                uint32_t idx = push_node(NodeType::Integer, curr_token(), { .i64 = val });
                next_token(); 
                return idx;
            }
            case TokenType::DECIMAL: 
            {
                Token tok = get_curr_token(); double val = 0.0;
                std::from_chars(tok.literal.data(), tok.literal.data() + tok.literal.size(), val);
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
            case TokenType::IDENT: 
            {
                uint32_t idx = push_node(NodeType::Identifier, curr_token()); 
                next_token();
                return idx;
            }
            case TokenType::BANG:
            case TokenType::MINUS: 
            {
                uint32_t signIdx = curr_token(); next_token(); 
                uint32_t expr = parse_expr(PrecedenceType::P_PREFIX);
                if (expr == ERROR_NODE) return ERROR_NODE;
                return push_node(NodeType::UnaryExpr, signIdx, { .indicies = { .lhs = expr, .rhs = 0 } });
            }
            case TokenType::LPAREN:
            {
                next_token(); 
                uint32_t expr = parse_expr(PrecedenceType::P_LOWEST);
                if (expr == ERROR_NODE) return ERROR_NODE;
                if (!curr_token_is(TokenType::RPAREN)) return add_error("Expected ')' after expression");
                next_token(); 
                return expr;
            }
            case TokenType::LABEL:
            {
                uint32_t label_tok = curr_token(); next_token();
                uint32_t label_node = push_node(NodeType::Label, label_tok);

                if (curr_token_is(TokenType::COLON))
                {
                    next_token(); // consume ':'
                    uint32_t expr = parse_expr(PrecedenceType::P_PREFIX);
                    if (expr == ERROR_NODE) return ERROR_NODE;

                    return push_node(NodeType::LabeledExpr, label_tok, { .indicies = { .lhs = label_node, .rhs = expr } });
                }

                return label_node;
            }
            case TokenType::AMPERSAND: return parse_borrow_expr();
            case TokenType::LBRACE: return parse_block_expr();
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
            case TokenType::GTE: 
                return parse_numeric_expr(left);
            case TokenType::DOT: 
                return parse_field_access_expr(left);
            case TokenType::LPAREN: 
                return parse_call_expr(left);
            case TokenType::LBRACKET: 
                return parse_index_expr(left);
            default: 
                return add_error("Unexpected token in binary/infix expression");
        }
    }

    uint32_t Parser::parse_numeric_expr(uint32_t left)
    {
        uint32_t op_idx = curr_token(); 
        const auto& token = get_curr_token(); next_token();
        PrecedenceType precedence = get_precedence(token.type);
        PrecedenceType right_precedence = precedence;
        
        if (token.type == TokenType::POW) 
            right_precedence = static_cast<PrecedenceType>(static_cast<uint8_t>(precedence) - 1);
        
        uint32_t right = parse_expr(right_precedence);
        if (right == ERROR_NODE) return ERROR_NODE;
        return push_node(NodeType::BinaryExpr, op_idx, { .indicies = { .lhs = left, .rhs = right } });
    }

    uint32_t Parser::parse_field_access_expr(uint32_t left)
    {
        uint32_t dot_tok = curr_token(); next_token();
        if (!curr_token_is(TokenType::IDENT)) return add_error("Expected field identifier after '.'");
        uint32_t field = push_node(NodeType::Identifier, curr_token()); next_token();
        return push_node(NodeType::FieldAccess, dot_tok, { .indicies = { .lhs = left, .rhs = field } });
    }

    uint32_t Parser::parse_call_expr(uint32_t left)
    {
        uint32_t paren_tok = curr_token(); next_token(); // consume '('
        uint32_t start = get_list_start();
        uint32_t count = 0;

        push_list(left, count);
        if (!curr_token_is(TokenType::RPAREN))
        {
            while (true)
            {
                uint32_t arg = parse_expr(PrecedenceType::P_LOWEST);
                if (arg == ERROR_NODE) return ERROR_NODE;
                push_list(arg, count);

                if (curr_token_is(TokenType::COMMA)) next_token();
                else if (curr_token_is(TokenType::RPAREN)) break;
                else return add_error("Expected ',' or ')' in function call argument list");
            }
        }
        next_token(); // consume ')'
        return push_node(NodeType::CallExpr, paren_tok, { .list = { .start = start, .count = count } });
    }

    uint32_t Parser::parse_index_expr(uint32_t left)
    {
        uint32_t bracket_tok = curr_token(); next_token(); // consume '['
        uint32_t index_expr = parse_expr(PrecedenceType::P_LOWEST);
        if (index_expr == ERROR_NODE) return ERROR_NODE;
        if (!curr_token_is(TokenType::RBRACKET)) return add_error("Expected ']' after index expression"); 
        next_token(); // consume ']'

        return push_node(NodeType::IndexExpr, bracket_tok, { .indicies = { .lhs = left, .rhs = index_expr } });
    }

    uint32_t Parser::parse_borrow_expr()
    {
        uint32_t amp_tok = curr_token(); next_token(); // consume '&'
        uint32_t expr = parse_expr(PrecedenceType::P_PREFIX);
        if (expr == ERROR_NODE) return ERROR_NODE;
        return push_node(NodeType::BorrowExpr, amp_tok, { .indicies = { .lhs = expr } });
    }

    uint32_t Parser::parse_block_expr()
    {
        uint32_t lbrace_tok = curr_token(); next_token(); // consume '{'
        uint32_t start = get_list_start();
        uint32_t count = 0;

        while (!curr_token_is(TokenType::RBRACE) && !curr_token_is(TokenType::END_OF_FILE))
        {
            uint32_t stmt = parse_statement();
            if (stmt == ERROR_NODE) synchronize();
            else push_list(stmt, count);
        }

        if (!curr_token_is(TokenType::RBRACE)) return add_error("Expected '}' at end of block expression");
        next_token(); // consume '}'

        return push_node(NodeType::BlockExpr, lbrace_tok, { .list = { .start = start, .count = count } });
    }

    // parse statement
    uint32_t Parser::parse_statement()
    {
        switch (get_curr_token().type)
        {
            case TokenType::LET:      return parse_let_stmt();
            case TokenType::RETURN:   return parse_return_stmt();
            case TokenType::BREAK:    return parse_break_stmt();
            case TokenType::CONTINUE: return parse_continue_stmt();
            default:                  return parse_expr_stmt();
        }
    }

    uint32_t Parser::parse_expr_stmt()
    {
        uint32_t tok = curr_token();
        uint32_t expr = parse_expr(PrecedenceType::P_LOWEST);
        if (expr == ERROR_NODE) return ERROR_NODE;

        if (curr_token_is(TokenType::SEMICOLON)) next_token();
        else if (m_Nodes[expr].type != NodeType::BlockExpr && 
             m_Nodes[expr].type != NodeType::LabeledExpr) return add_error("Expected ';' after expression statement");

        return push_node(NodeType::ExprStmt, tok, { .indicies = { .lhs = expr } });
    }


    uint32_t Parser::parse_let_stmt() // let [ident]: mut [type] = [expr];
    {
        uint32_t let_tok = curr_token(); next_token(); 
        if (!curr_token_is(TokenType::IDENT)) return add_error("Expected identifier after 'let'");
        uint32_t ident = push_node(NodeType::Identifier, curr_token()); next_token(); 

        uint32_t type_annotation = ERROR_NODE;
        if (curr_token_is(TokenType::COLON))
        {
            next_token(); 
            type_annotation = parse_type_annotation();
            if (type_annotation == ERROR_NODE) return ERROR_NODE;
        }
        if (!curr_token_is(TokenType::EQ)) return add_error("Expected '=' in variable declaration");
        next_token(); // consume '='

        uint32_t expr = parse_expr(PrecedenceType::P_LOWEST);
        if (expr == ERROR_NODE) return ERROR_NODE;

        if (!curr_token_is(TokenType::SEMICOLON)) return add_error("Expected ';' after let statement"); 
        next_token(); // consume ';'

        uint32_t start = get_list_start(), count = 0;
        push_list(ident, count);
        push_list(type_annotation, count);
        push_list(expr, count);

        return push_node(NodeType::LetStmt, let_tok, { .list = { .start = start, .count = count } });
    }
    
    uint32_t Parser::parse_return_stmt() // return [expr];
    {
        uint32_t ret_tok = curr_token(); next_token(); // consume 'return'
        uint32_t expr = ERROR_NODE;

        if (!curr_token_is(TokenType::SEMICOLON))
        {
            expr = parse_expr(PrecedenceType::P_LOWEST);
            if (expr == ERROR_NODE) return ERROR_NODE;
        }

        if (!curr_token_is(TokenType::SEMICOLON)) return add_error("Expected ';' after return statement"); 
        next_token(); // consume ';'

        return push_node(NodeType::ReturnStmt, ret_tok, { .indicies = { .lhs = expr } });
    }

    uint32_t Parser::parse_break_stmt() // break [expr];
    {   
        uint32_t break_tok = curr_token(); next_token(); // consume 'break'
        uint32_t label = ERROR_NODE;
        uint32_t expr = ERROR_NODE;

        if (curr_token_is(TokenType::SEMICOLON)) 
        {
            next_token(); // consume ';'
            return push_node(NodeType::BreakStmt, break_tok);
        }

        if (curr_token_is(TokenType::LABEL))
        {
            label = push_node(NodeType::Label, curr_token());
            next_token();
        }

        if (!curr_token_is(TokenType::SEMICOLON))
        {
            expr = parse_expr(PrecedenceType::P_LOWEST);
            if (expr == ERROR_NODE) return ERROR_NODE;
        }

        if (!curr_token_is(TokenType::SEMICOLON)) return add_error("Expected ';' after break statement"); 
        next_token(); // consume ';'

        return push_node(NodeType::BreakStmt, break_tok, { .indicies = { .lhs = label, .rhs = expr } });
    }   

    uint32_t Parser::parse_continue_stmt() // continue;
    {
        uint32_t cont_tok = curr_token(); next_token(); // consume 'continue'
        uint32_t expr = ERROR_NODE;

        if (!curr_token_is(TokenType::SEMICOLON)) return add_error("Expected ';' after continue statement"); 
        next_token(); // consume ';'

        return push_node(NodeType::ContinueStmt, cont_tok);
    }

    // other

    uint32_t Parser::parse_type_annotation() // &mut [ident] / mut [ident] / [ident]
    {
        uint32_t is_borrow = 0, is_mut = 0;
        uint32_t first_tok = curr_token();

        if (curr_token_is(TokenType::AMPERSAND)) { is_borrow = 1; next_token(); } // &
        if (curr_token_is(TokenType::MUT)) { is_mut = 1; next_token(); } // mut

        if (!curr_token_is(TokenType::IDENT)) return add_error("Expected type identifier in type annotation");
        next_token();

        return push_node(NodeType::TypeAnnotation, first_tok, { .indicies = { .lhs = is_borrow, .rhs = is_mut } });
    }

    void Parser::synchronize()
    {
        while (!curr_token_is(TokenType::END_OF_FILE))
        {
            if (curr_token_is(TokenType::SEMICOLON))
            {
                next_token(); // consume ';'
                return;
            }

            if (curr_token_is(TokenType::RBRACE)) return;
            next_token();
        }
    }

    uint32_t Parser::add_error(std::string_view message)
    {
        const Token& tok = get_curr_token();
        std::string err = std::format("[Parse Error] Failed at line: {}, column: {} ({}): {}", 
                                    tok.line, tok.column, tok.literal, message);
        m_Errors.push_back(err);
        return ERROR_NODE;
    }
}
