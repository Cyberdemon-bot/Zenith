#include <algorithm>
#include <array>
#include "Zenith/Compiler/Lexer.h"

namespace Zenith {
    namespace 
    {
        struct KeyPair 
        {
            std::string_view name;
            TokenType type;
        };

        constexpr std::array<KeyPair, 23> KEYWORDS = {{
            {"and",      TokenType::AND},
            {"as",       TokenType::AS},
            {"break",    TokenType::BREAK},
            {"class",    TokenType::CLASS},
            {"continue", TokenType::CONTINUE},
            {"dyn",      TokenType::DYN},
            {"else",     TokenType::ELSE},
            {"false",    TokenType::FALSE},
            {"for",      TokenType::FOR},
            {"func",     TokenType::FUNC}, 
            {"if",       TokenType::IF},
            {"in",       TokenType::IN},
            {"let",      TokenType::LET},
            {"match",    TokenType::MATCH},
            {"mut",      TokenType::MUT},
            {"native",   TokenType::NATIVE},
            {"new",      TokenType::NEW},
            {"null",     TokenType::NULL_LIT},
            {"or",       TokenType::OR},
            {"pub",      TokenType::PUB},
            {"return",   TokenType::RETURN},
            {"struct",   TokenType::STRUCT},
            {"true",     TokenType::TRUE}
        }};

        static_assert(std::ranges::is_sorted(KEYWORDS, {}, &KeyPair::name), "KEYWORDS must stay sorted");

        TokenType LookupIdent(std::string_view ident) 
        {
            auto it = std::lower_bound
            (
                KEYWORDS.begin(), KEYWORDS.end(), ident,
                [](const KeyPair& pair, std::string_view val) 
                { 
                    return pair.name < val; 
                }
            );
            if (it != KEYWORDS.end() && it->name == ident) return it->type;
            return TokenType::IDENT;
        }
    }

    void Lexer::Reset(std::string_view source)
    {
        m_Source = source;
        m_Pos = 0;
        m_ReadPos = 0;
        m_Line = 1;
        m_LineStartPos = 0;
        m_CurrChar = NONE_CHAR;
        m_HadUnterminatedComment = false;
        ReadChar();
    }

    Token Lexer::NextToken()
    {
        SkipWhitespace(); 

        if (m_HadUnterminatedComment)
        {
            m_HadUnterminatedComment = false;
            return NewToken(TokenType::ILLEGAL, "", m_Pos);
        }

        if (m_CurrChar == NONE_CHAR) return NewToken(TokenType::END_OF_FILE, "", m_Pos); 
        uint32_t startPos = m_Pos;

        if (IsLetter(m_CurrChar)) return ReadIdentifier(); 
        if (IsDigit(m_CurrChar))  return ReadNumber();     
        if (m_CurrChar == '"')     return ReadString();    
        if (m_CurrChar == '\'')    return ReadLabel();

        switch (m_CurrChar) 
        {
            case ';': ReadChar(); return NewToken(TokenType::SEMICOLON, GetLil(startPos), startPos); 
            case ':': ReadChar(); return NewToken(TokenType::COLON,     GetLil(startPos), startPos); 
            case ',': ReadChar(); return NewToken(TokenType::COMMA,     GetLil(startPos), startPos); 
            case '.': ReadChar(); return NewToken(TokenType::DOT,       GetLil(startPos), startPos); 
            case '?': ReadChar(); return NewToken(TokenType::QUESTION,  GetLil(startPos), startPos); 
            case '&': ReadChar(); return NewToken(TokenType::AMPERSAND, GetLil(startPos), startPos); 
            
            case '(': ReadChar(); return NewToken(TokenType::LPAREN,    GetLil(startPos), startPos); 
            case ')': ReadChar(); return NewToken(TokenType::RPAREN,    GetLil(startPos), startPos); 
            case '{': ReadChar(); return NewToken(TokenType::LBRACE,    GetLil(startPos), startPos); 
            case '}': ReadChar(); return NewToken(TokenType::RBRACE,    GetLil(startPos), startPos); 
            case '[': ReadChar(); return NewToken(TokenType::LBRACKET,  GetLil(startPos), startPos); 
            case ']': ReadChar(); return NewToken(TokenType::RBRACKET,  GetLil(startPos), startPos); 

            case '+': ReadChar(); return NewToken(TokenType::PLUS,      GetLil(startPos), startPos); 
            case '-':
            {
                if (PeekChar() == '>')
                {
                    ReadChar(); ReadChar();
                    return NewToken(TokenType::ARROW, GetLil(startPos), startPos);
                }
                ReadChar();
                return NewToken(TokenType::MINUS, GetLil(startPos), startPos);
            }
            case '*':
            {
                ReadChar();
                return NewToken(TokenType::ASTERISK, GetLil(startPos), startPos);
            }
            case '/': ReadChar(); return NewToken(TokenType::SLASH,     GetLil(startPos), startPos); 
            case '%': ReadChar(); return NewToken(TokenType::MODULUS,   GetLil(startPos), startPos); 
            
            case '=':
            {
                if (PeekChar() == '=') 
                {
                    ReadChar(); ReadChar(); 
                    return NewToken(TokenType::EE, GetLil(startPos), startPos); 
                }
                if (PeekChar() == '>')
                {
                    ReadChar(); ReadChar();
                    return NewToken(TokenType::FATARROW, GetLil(startPos), startPos);
                }
                ReadChar(); 
                return NewToken(TokenType::EQ, GetLil(startPos), startPos); 
            }
            case '!':
            {
                if (PeekChar() == '=')
                {
                    ReadChar(); ReadChar();
                    return NewToken(TokenType::NE, GetLil(startPos), startPos);
                }
                ReadChar();
                return NewToken(TokenType::BANG, GetLil(startPos), startPos);
            }
            case '<':
            {
                if (PeekChar() == '=') 
                {
                    ReadChar(); ReadChar(); 
                    return NewToken(TokenType::LTE, GetLil(startPos), startPos); 
                }
                ReadChar(); 
                return NewToken(TokenType::LT, GetLil(startPos), startPos); 
            }
            case '>':
            {
                if (PeekChar() == '=') 
                {
                    ReadChar(); ReadChar(); 
                    return NewToken(TokenType::GTE, GetLil(startPos), startPos); 
                }
                ReadChar(); 
                return NewToken(TokenType::GT, GetLil(startPos), startPos); 
            }
        }

        ReadChar(); 
        return NewToken(TokenType::ILLEGAL, GetLil(startPos), startPos); 
    }

    void Lexer::ReadChar()
    {
        if (m_ReadPos >= m_Source.size())
        {
            m_CurrChar = NONE_CHAR;
            m_Pos = static_cast<uint32_t>(m_Source.size());
        }
        else
        {
            m_CurrChar = m_Source[m_ReadPos];
            m_Pos = m_ReadPos;
            ++m_ReadPos;
        }
    }

    char Lexer::PeekChar()
    {   
        if (m_ReadPos >= m_Source.size()) return NONE_CHAR;
        return m_Source[m_ReadPos];
    }

    void Lexer::SkipWhitespace()
    {
        while (true) 
        {
            switch (m_CurrChar) 
            {
                case ' ':
                case '\t':
                case '\r': 
                { 
                    ReadChar(); 
                    break; 
                }
                case '\n': 
                {
                    m_Line++;
                    ReadChar();
                    m_LineStartPos = m_Pos;
                    break;
                }
                case '/':
                {
                    if (PeekChar() == '/')
                    {
                        while (m_CurrChar != '\n' && m_CurrChar != NONE_CHAR) ReadChar();
                    }
                    else if (PeekChar() == '*')
                    {
                        ReadChar(); ReadChar(); 
                        uint32_t depth = 1;
                        while (m_CurrChar != NONE_CHAR && depth > 0)
                        {
                            if (m_CurrChar == '\n')
                            {
                                m_Line++;
                                ReadChar();
                                m_LineStartPos = m_Pos;
                            }
                            else if (m_CurrChar == '/' && PeekChar() == '*')
                            {
                                depth++;
                                ReadChar(); ReadChar();
                            }
                            else if (m_CurrChar == '*' && PeekChar() == '/')
                            {
                                depth--;
                                ReadChar(); ReadChar();
                            }
                            else ReadChar();
                        }
                        if (depth > 0) m_HadUnterminatedComment = true;
                    }
                    else return;
                    break;
                }
                default: return;
            }
        }
    }

    Token Lexer::ReadIdentifier()
    {
        uint32_t startPos = m_Pos;
        while (m_CurrChar != NONE_CHAR && IsAlnum(m_CurrChar)) ReadChar();
        auto lil = GetLil(startPos);
        auto type = LookupIdent(lil);
        return NewToken(type, lil, startPos);
    }

    Token Lexer::ReadNumber()
    {
        uint32_t startPos = m_Pos;
        bool isDecimal = false;
        while (IsDigit(m_CurrChar)) ReadChar();

        if (m_CurrChar == '.' && IsDigit(PeekChar()))
        {
            isDecimal = true;
            ReadChar();
            while (IsDigit(m_CurrChar)) ReadChar();
        }

        if (m_CurrChar == 'e' || m_CurrChar == 'E')
        {
            char nextChar = PeekChar();
            if (IsDigit(nextChar))
            {
                isDecimal = true;
                ReadChar();
                while (IsDigit(m_CurrChar)) ReadChar();
            }
            else if (nextChar == '+' || nextChar == '-')
            {
                uint32_t afterSign = m_ReadPos + 1;
                if (afterSign < m_Source.size() && IsDigit(m_Source[afterSign]))
                {
                    isDecimal = true;
                    ReadChar();
                    ReadChar();
                    while (IsDigit(m_CurrChar)) ReadChar();
                }
            }
        }

        return NewToken(isDecimal ? TokenType::DECIMAL : TokenType::INTEGER, GetLil(startPos), startPos);
    }

    Token Lexer::ReadString()
    {
        uint32_t contentStart = m_Pos + 1;
        uint32_t startLine = m_Line;
        uint32_t startCol = GetColumn(contentStart);
        bool terminated = false;

        while (true)
        {
            ReadChar();
            if (m_CurrChar == NONE_CHAR) break;
            if (m_CurrChar == '"') { terminated = true; break; }
            if (m_CurrChar == '\n')
            {
                m_Line++;
                m_LineStartPos = m_Pos + 1;
                continue;
            }
            if (m_CurrChar == '\\')
            {
                ReadChar();
                if (m_CurrChar == NONE_CHAR) break;
                if (m_CurrChar == '\n')
                {
                    m_Line++;
                    m_LineStartPos = m_Pos + 1;
                }
            }
        }

        auto lil = GetLil(contentStart);
        if (terminated) ReadChar();

        return Token
        {
            .type = terminated ? TokenType::STRING : TokenType::ILLEGAL,
            .literal = lil,
            .line = startLine,
            .column = startCol
        };
    }

    Token Lexer::ReadLabel()
    {
        ReadChar(); 
        if (!IsLetter(m_CurrChar)) return NewToken(TokenType::ILLEGAL, "", m_Pos);
        uint32_t startPos = m_Pos;
        while (m_CurrChar != NONE_CHAR && IsAlnum(m_CurrChar)) ReadChar();
        return NewToken(TokenType::LABEL, GetLil(startPos), startPos);
    }
}
