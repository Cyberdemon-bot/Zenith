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
        std::string_view m_Source;
        uint32_t m_Pos = 0, m_ReadPos = 0, m_Line = 1, m_LineStartPos = 0;
        char m_CurrChar = NONE_CHAR;
        bool m_HadUnterminatedComment = false;

        void ReadChar();
        char PeekChar();
        void SkipWhitespace();

        Token ReadIdentifier();
        Token ReadNumber();
        Token ReadString();
        Token ReadLabel();

        static constexpr bool IsDigit(char ch) { return ch >= '0' && ch <= '9'; }
        static constexpr bool IsLetter(char ch) { return ('a' <= ch && ch <= 'z') || ('A' <= ch && ch <= 'Z') || ch == '_'; }
        static constexpr bool IsAlnum(char ch) { return IsLetter(ch) || IsDigit(ch); }

        constexpr uint32_t GetColumn(uint32_t startPos) const 
        {
            return startPos - m_LineStartPos + 1;
        }

        constexpr std::string_view GetLil(uint32_t startPos) const
        {
            return std::string_view(m_Source).substr(startPos, m_Pos - startPos);
        }

        constexpr Token NewToken(TokenType type, std::string_view literal, uint32_t startPos)
        {
            return Token
            { 
                .type = type, 
                .literal = literal, 
                .line = m_Line,
                .column = GetColumn(startPos)
            };
        }
    };
}
