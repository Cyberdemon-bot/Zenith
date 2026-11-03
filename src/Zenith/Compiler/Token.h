#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace Zenith {

    enum class TokenType : uint8_t
    {
        END_OF_FILE, ILLEGAL,
        IDENT, INTEGER, DECIMAL, STRING, LABEL,
        // +      -         *      /        %    
        PLUS, MINUS, ASTERISK, SLASH, MODULUS,
        EQ, LT, LTE, GT, GTE, EE, NE, // EE -> ==, NE -> !=
        BANG, QUESTION, AMPERSAND, DOT, // !, ?, &, .
        ARROW, FATARROW, // ->, =>

        SEMICOLON, COLON, COMMA, LPAREN, RPAREN, LBRACE, RBRACE, LBRACKET, RBRACKET,
        BREAK, CLASS, CONTINUE, DYN, ELSE, FOR, FUNC, IF, LET, MATCH, MUT, NATIVE, NEW, PUB, RETURN, STRUCT,
        AND, OR, IN,
        FALSE, NULL_LIT, TRUE,
        COUNT
    };


    inline constexpr std::array<std::string_view, static_cast<size_t>(TokenType::COUNT)> TOKEN_NAMES = {
        "END_OF_FILE", "ILLEGAL",
        "IDENT", "INTEGER", "DECIMAL", "STRING", "LABEL",

        "PLUS", "MINUS", "ASTERISK", "SLASH", "MODULUS",
        "EQ", "LT", "LTE", "GT", "GTE", "EE", "NE",
        "BANG", "QUESTION", "AMPERSAND", "DOT", 
        "ARROW", "FATARROW",

        "SEMICOLON", "COLON", "COMMA", "LPAREN", "RPAREN", "LBRACE", "RBRACE", "LBRACKET", "RBRACKET",

        "BREAK", "CLASS", "CONTINUE", "DYN", "ELSE", "FOR", "FUNC", "IF", "LET", "MATCH", "MUT", "NATIVE", "NEW", "PUB", "RETURN", "STRUCT",
        "AND", "OR", "IN",
        "FALSE", "NULL", "TRUE"
    };

    struct Token
    {
        TokenType type = TokenType::ILLEGAL;
        std::string_view literal;
        uint32_t line, column;

        constexpr std::string_view TypeToString() const
        {
            auto index = static_cast<std::size_t>(type);
            if (index < TOKEN_NAMES.size()) return TOKEN_NAMES[index];
            return "UNKNOWN";
        }
    };
}