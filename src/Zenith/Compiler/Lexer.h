#pragma once

#include <string_view>
#include "Zenith/Compiler/Token.h"

namespace Zenith {
    inline constexpr char NONE_CHAR = '\0';

    class Lexer
    {
    public:
        Lexer(std::string_view source);
        Token NextToken();
    private:
        std::string_view source;
        uint32_t pos = 0, read_pos = 0, line = 1, line_start_pos = 0;
        char curr_char = NONE_CHAR;
        bool m_HadUnterminatedComment = false;

        void read_char();
        char peek_char();
        void skip_whitespace();

        Token read_identifier();
        Token read_number();
        Token read_string();
        Token read_label();

        static constexpr bool is_digit(char ch) { return ch >= '0' && ch <= '9'; }
        static constexpr bool is_letter(char ch) { return ('a' <= ch && ch <= 'z') || ('A' <= ch && ch <= 'Z') || ch == '_'; }
        static constexpr bool is_alnum(char ch) { return is_letter(ch) || is_digit(ch); }

        constexpr uint32_t get_column(uint32_t start_pos) const 
        {
            return start_pos - line_start_pos + 1;
        }

        constexpr std::string_view get_lil(uint32_t start_pos) const
        {
            return std::string_view(source).substr(start_pos, pos - start_pos);
        }

        constexpr Token new_token(TokenType type, std::string_view literal, uint32_t start_pos)
        {
            return Token
            { 
                .type = type, 
                .literal = literal, 
                .line = line,
                .column = get_column(start_pos)
            };
        }
    };
}