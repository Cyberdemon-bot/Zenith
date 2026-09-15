#pragma once

#include <string_view>

namespace Zenith {

    enum class TokenType : uint8_t
    {
        END_OF_FILE, ILLEGAL,
        IDENT, INTEGER, DECIMAL, STRING, 
        // +      -         *      /        %    ^
        PLUS, MINUS, ASTERISK, SLASH, MODULUS, POW,
        EQ, LT, LTE, GT, GTE, EE, NE, // EE -> ==, NE -> !=
        BANG, ARROW,

        SEMICOLON, COLON, COMMA, LPAREN, RPAREN, LBRACE, RBRACE, LBRACKET, RBRACKET,
        
        LET, FUNCTION, WHILE, FOR,
        IF, ELSE, RETURN, CONTINUE, BREAK,

        COUNT
    };

    inline constexpr std::array<std::string_view, static_cast<size_t>(TokenType::COUNT)> TOKEN_NAMES = {
        "END_OF_FILE", "ILLEGAL",
        "IDENT", "INTEGER", "DECIMAL", "STRING", 
        "PLUS", "MINUS", "ASTERISK", "SLASH", "MODULUS", "POW",
        "EQ", "LT", "LTE", "GT", "GTE", "EE", "NE",
        "BANG", "ARROW",
        "SEMICOLON", "COLON", "COMMA", "LPAREN", "RPAREN", "LBRACE", "RBRACE",
        "LBRACKET", "RBRACKET",
        "LET", "FUNCTION", "WHILE", "FOR",
        "IF", "ELSE", "RETURN", "CONTINUE", "BREAK"
    };

    struct Token
    {
        TokenType type;
        std::string_view literal;
        uint32_t line, column;

        constexpr std::string_view type2string() const
        {
            auto index = static_cast<std::size_t>(type);
            if (index < TOKEN_NAMES.size()) return TOKEN_NAMES[index];
            return "UNKNOWN";
        }
    };
}