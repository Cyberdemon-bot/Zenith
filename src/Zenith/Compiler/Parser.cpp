#include <charconv>
#include <iostream>
#include <iomanip>
#include "Zenith/Compiler/Parser.h"

namespace Zenith {

    Parser::Parser(std::string_view source)
        : m_Lexer(source)
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
        return parse_expr(Zenith::PrecedenceType::P_LOWEST);
    }

    uint32_t Parser::parse_prefix() 
    {
        const auto& curr = get_curr_token();
        switch (curr.type) 
        {
            case TokenType::INTEGER: 
            {
                uint32_t idx = make_integer(curr_token()); next_token(); 
                return idx;
            }
            case TokenType::DECIMAL: 
            {
                uint32_t idx = make_decimal(curr_token()); next_token(); 
                return idx;
            }
            case TokenType::IDENT: 
            {
                uint32_t idx = make_identifier(curr_token()); next_token();
                return idx;
            }
            case TokenType::MINUS: 
            {
                uint32_t signIdx = curr_token(); next_token(); 
                uint32_t expr = parse_expr(PrecedenceType::P_PREFIX);
                if (expr == ERROR_NODE) return ERROR_NODE;
                return make_unary(signIdx, expr);
            }
            case TokenType::LPAREN:
            {
                next_token(); 
                uint32_t expr = parse_expr(PrecedenceType::P_LOWEST);
                if (expr == ERROR_NODE) return ERROR_NODE;
                if (!curr_token_is(TokenType::RPAREN)) return ERROR_NODE;
                next_token(); 
                return expr;
            }
            default: return ERROR_NODE;
        }
    }

    uint32_t Parser::parse_infix(uint32_t left)
    {
        uint32_t op_idx = curr_token();
        const auto& op_token = get_curr_token();

        PrecedenceType precedence = get_precedence(op_token.type);
        PrecedenceType right_precedence = precedence;
        if (op_token.type == TokenType::POW) right_precedence = 
                    static_cast<PrecedenceType>(static_cast<uint8_t>(precedence) - 1);
        next_token();
        uint32_t right = parse_expr(right_precedence);
        if (right == ERROR_NODE) return ERROR_NODE;
        return make_binary(op_idx, left, right);
    }

    uint32_t Parser::parse_expr(PrecedenceType precedence)
    {
        uint32_t left = parse_prefix();
        if (left == ERROR_NODE) return ERROR_NODE;
        while (!curr_token_is(TokenType::SEMICOLON) && 
           precedence < get_precedence(get_curr_token().type))
        {
            left = parse_infix(left);
            if (left == ERROR_NODE) return ERROR_NODE;
        }

        return left;
    }

    uint32_t Parser::make_binary(uint32_t token, uint32_t left, uint32_t right)
    {
        Node binary = Node
        {
            .type = NodeType::BinaryExpr,
            .token = token,
            .data = { .u32 = { .lhs = left, .rhs = right } }
        };
        m_Nodes.push_back(binary);
        return get_curr_node();
    }

    uint32_t Parser::make_unary(uint32_t token, uint32_t child)
    {
        Node unary = Node
        {
            .type = NodeType::UnaryExpr,
            .token = token,
            .data = { .u32 = { .lhs = child } }
        };
        m_Nodes.push_back(unary);
        return get_curr_node();
    }

    uint32_t Parser::make_identifier(uint32_t token)
    {
        Node ident = Node
        {
            .type = NodeType::Identifier,
            .token = token
        };
        m_Nodes.push_back(ident);
        return get_curr_node();
    }

    uint32_t Parser::make_integer(uint32_t token)
    {
        Token tok = get_token(token); int64_t val = 0;
        std::from_chars(tok.literal.data(), tok.literal.data() + tok.literal.size(), val);
        m_Nodes.push_back(Node
        {
            .type = NodeType::Integer,
            .token = token,
            .data = { .i64 = val }
        });
        return get_curr_node();
    }

    uint32_t Parser::make_decimal(uint32_t token)
    {
        Token tok = get_token(token); double val = 0.0;
        std::from_chars(tok.literal.data(), tok.literal.data() + tok.literal.size(), val);
        m_Nodes.push_back(Node
        {
            .type = NodeType::Decimal,
            .token = token,
            .data = { .f64 = val }
        });
        return get_curr_node();
    }

    void Parser::dump_ast() const 
    {
        std::cout << "\n--- FLAT AST DUMP (" << m_Nodes.size() << " nodes) ---\n";
        for (size_t i = 0; i < m_Nodes.size(); ++i) 
        {
            const auto& node = m_Nodes[i];
            const auto& tok = m_Tokens[node.token];

            std::cout << "Node[" << std::setfill('0') << std::setw(2) << i << "] "
                    << "| Tok[" << std::setw(2) << node.token << "]: '" << tok.literal << "' ";

            switch (node.type) 
            {
                case NodeType::Integer:
                    std::cout << "| [Integer]  Val: " << node.data.i64;
                    break;
                case NodeType::Decimal:
                    std::cout << "| [Decimal]  Val: " << node.data.f64;
                    break;
                case NodeType::Identifier:
                    std::cout << "| [Ident]    Name: " << tok.literal;
                    break;
                case NodeType::UnaryExpr:
                    std::cout << "| [Unary]    Child: " << node.data.u32.lhs;
                    break;
                case NodeType::BinaryExpr:
                    std::cout << "| [Binary]   LHS: " << std::setw(2) << node.data.u32.lhs 
                            << " | RHS: " << std::setw(2) << node.data.u32.rhs;
                    break;
                default:
                    std::cout << "| [Unknown]";
                    break;
            }
            std::cout << "\n";
        }
    }
}