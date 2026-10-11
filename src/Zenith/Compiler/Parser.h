#pragma once

#include <string>
#include <vector>
#include <string_view>
#include "Zenith/Compiler/Node.h"
#include "Zenith/Compiler/Lexer.h"

namespace Zenith {

    constexpr uint32_t ERROR_NODE = 0;

    enum class ErrorScope : uint8_t
    {
        Token,       
        NodeStart,    
        TokenBefore, 
    };

    struct ParseError
    {
        std::string message;
        uint32_t node  = ERROR_NODE;
        uint32_t token = 0;
        ErrorScope scope = ErrorScope::Token;
    };

    struct ListHeader 
    {
        uint32_t start;
        uint32_t count;
    };

    struct StructLitScope
    {
        bool& flag;
        bool saved;
        StructLitScope(bool& f, bool restrict_literals) : flag(f), saved(f) { flag = restrict_literals; }
        ~StructLitScope() { flag = saved; }
    };

    enum class PrecedenceType : uint8_t 
    {
        P_LOWEST = 0,
        P_OR,
        P_AND,
        P_EQUALS,
        P_LESSGREATER,
        P_SUM,
        P_PRODUCT,
        P_UNARY,
        P_CALL,
        P_INDEX
    };

    class Parser 
    {
    public:
        Parser(std::string_view source);
        uint32_t parse_program();
        void dump_ast() const;
        void dump_errors() const;

        bool has_errors() const { return !m_Errors.empty(); }
        uint32_t get_errors_count() const { return static_cast<uint32_t>(m_Errors.size()); }
    private:
        std::string_view m_Source;
        uint32_t m_Cursor = 0;
        std::vector<Node> m_Nodes;
        std::vector<Token> m_Tokens;
        std::vector<uint32_t> m_Extra;
        std::vector<uint32_t> m_Scratch;
        std::vector<ParseError> m_Errors;
        Lexer m_Lexer;
        bool m_NoStructLit = false;

        // error handler
        void synchronize();
        uint32_t add_error(std::string_view message, uint32_t node = ERROR_NODE, ErrorScope scope = ErrorScope::Token);

        // buffer's methods
        size_t get_scratch_marker() const { return m_Scratch.size(); }
        void push_scratch(uint32_t node) { m_Scratch.push_back(node); }
        void rollback_scratch(size_t marker) { m_Scratch.resize(marker); }
        ListHeader commit_list(size_t marker);

        uint32_t next_token() { return ++m_Cursor; }
        uint32_t curr_token() { return m_Cursor; }
        const Token& get_token(uint32_t idx) const { return m_Tokens[idx]; }

        const Token& get_curr_token() const { return get_token(m_Cursor); }
        const Token& get_peek_token() const { return get_token(m_Cursor + 1); }
        TokenType peek_type(uint32_t n) const
        {
            size_t idx = m_Cursor + n;
            if (idx >= m_Tokens.size()) idx = m_Tokens.size() - 1;
            return m_Tokens[idx].type;
        }
        bool curr_token_is(TokenType type) { return get_token(m_Cursor).type == type; }
        bool peek_token_is(TokenType type) { return get_token(m_Cursor + 1).type == type; }

        const Node& get_node(uint32_t idx) const { return m_Nodes[idx]; }
        uint32_t get_curr_node() { return static_cast<uint32_t>(m_Nodes.size() - 1); }
        uint32_t get_list_start() { return static_cast<uint32_t>(m_Extra.size()); }
        void push_list(uint32_t elem, uint32_t& count) { m_Extra.push_back(elem); ++count; }

        uint32_t push_node(NodeType type, uint32_t start, uint32_t end, Node::Data data = {}) 
        {
            m_Nodes.push_back(Node{ .type = type, .start_token = start, .end_token = end, .data = data });
            return static_cast<uint32_t>(m_Nodes.size() - 1);
        }

        uint32_t push_node(NodeType type, uint32_t start, Node::Data data = {}) 
        {
            m_Nodes.push_back(Node{ .type = type, .start_token = start, .end_token = start, .data = data });
            return static_cast<uint32_t>(m_Nodes.size() - 1);
        }

        // parsing methods
        uint32_t parse_statement();
        uint32_t default_parser();
        uint32_t parse_let_stmt();
        uint32_t parse_return_stmt();
        uint32_t parse_break_stmt();
        uint32_t parse_continue_stmt();
        uint32_t parse_assign_stmt(uint32_t lhs);
        uint32_t parse_func_decl(uint32_t start);
        uint32_t parse_typed_ident(std::string_view missing_name_message);
        uint32_t parse_struct_decl(uint32_t start);

        uint32_t parse_prefix_expr();
        uint32_t parse_infix_expr(uint32_t left);
        uint32_t parse_expr(PrecedenceType precedence);

        uint32_t parse_numeric_expr(uint32_t left);
        uint32_t parse_field_access_expr(uint32_t left);
        uint32_t parse_call_expr(uint32_t left);
        uint32_t parse_index_expr(uint32_t left);
        uint32_t parse_borrow_expr(uint32_t start, uint8_t is_mut);
        uint32_t parse_mut_expr();
        uint32_t parse_new_expr(uint32_t start, uint8_t is_mut);
        uint32_t parse_paren_expr();
        uint32_t parse_struct_init();
        uint32_t parse_field_init();
        uint32_t parse_arrow_body();
        uint32_t parse_block_expr();
        uint32_t parse_if_expr();
        uint32_t parse_for_expr();

        uint32_t parse_identifier();
        uint32_t parse_type_annotation();
        uint32_t parse_ref_type(uint32_t start, uint8_t is_mut);
        uint32_t parse_named_type();
        uint32_t parse_tuple_type();

        bool is_tuple_ahead() const;
        bool is_struct_init_ahead() const;

        //utils
        constexpr PrecedenceType get_precedence(TokenType type) noexcept 
        {
            switch (type) 
            {
                case TokenType::OR:         return PrecedenceType::P_OR;
                case TokenType::AND:        return PrecedenceType::P_AND;
                case TokenType::EE:         
                case TokenType::NE:         return PrecedenceType::P_EQUALS;
                case TokenType::LT:
                case TokenType::GT:
                case TokenType::LTE:
                case TokenType::GTE:        return PrecedenceType::P_LESSGREATER;
                case TokenType::PLUS:
                case TokenType::MINUS:      return PrecedenceType::P_SUM;
                case TokenType::ASTERISK:
                case TokenType::SLASH:
                case TokenType::MODULUS:    return PrecedenceType::P_PRODUCT;
                case TokenType::DOT:
                case TokenType::LPAREN:     return PrecedenceType::P_CALL;
                case TokenType::LBRACKET:   return PrecedenceType::P_INDEX;
                default:                    return PrecedenceType::P_LOWEST;
            }
        }

        template <typename Fn>
        uint32_t parse_delimited_list(
            NodeType res_type, 
            TokenType close_type, 
            TokenType sep_type, 
            Fn parse_element, 
            std::string_view error_message,
            bool allow_trailing = true)
        {
            uint32_t start = curr_token(); next_token(); // consume open_token
            size_t marker = get_scratch_marker();

            if (!curr_token_is(close_type))  
            {
                while (true)
                {
                    uint32_t elem = parse_element();
                    if (elem == ERROR_NODE)
                    {
                        rollback_scratch(marker);
                        return ERROR_NODE;
                    }
                    push_scratch(elem);
                    if (curr_token_is(sep_type))
                    {
                        next_token(); // consume separator
                        if (allow_trailing && curr_token_is(close_type)) break; // trailing: (1, 2, )
                    }
                    else if (curr_token_is(close_type)) break; 
                    else
                    {
                        rollback_scratch(marker);
                        return add_error(error_message);
                    }
                }
            }

            uint32_t end = curr_token(); next_token(); // consume close_type
            auto list = commit_list(marker);
            return push_node(res_type, start, end, { .list = { .start = list.start, .count = list.count } });
        }
    };
}