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

        constexpr std::array<KeyPair, 15> KEYWORDS = {{
            {"break",    TokenType::BREAK},
            {"class",    TokenType::CLASS},
            {"continue", TokenType::CONTINUE},
            {"else",     TokenType::ELSE},
            {"for",      TokenType::FOR},
            {"func",     TokenType::FUNC}, 
            {"if",       TokenType::IF},
            {"let",      TokenType::LET},
            {"match",    TokenType::MATCH},
            {"mut",      TokenType::MUT},
            {"native",   TokenType::NATIVE},
            {"new",      TokenType::NEW},
            {"pub",      TokenType::PUB},
            {"return",   TokenType::RETURN},
            {"struct",   TokenType::STRUCT}
        }};

        TokenType lookup_ident(std::string_view ident) 
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

    Lexer::Lexer(std::string_view source)
        : source(source)
    {
        read_char();
    }

    Token Lexer::next_token()
    {
        skip_whitespace(); 
        if (curr_char == NONE_CHAR) return new_token(TokenType::END_OF_FILE, "", pos); 
        uint32_t start_pos = pos;

        if (is_letter(curr_char)) return read_identifier(); 
        if (is_digit(curr_char))  return read_number();     
        if (curr_char == '"')     return read_string();    
        if (curr_char == '\'')    return read_label();

        switch (curr_char) 
        {
            case ';': read_char(); return new_token(TokenType::SEMICOLON, get_lil(start_pos), start_pos); 
            case ':': read_char(); return new_token(TokenType::COLON,     get_lil(start_pos), start_pos); 
            case ',': read_char(); return new_token(TokenType::COMMA,     get_lil(start_pos), start_pos); 
            case '.': read_char(); return new_token(TokenType::DOT,       get_lil(start_pos), start_pos); 
            case '?': read_char(); return new_token(TokenType::QUESTION,  get_lil(start_pos), start_pos); 
            case '&': read_char(); return new_token(TokenType::AMPERSAND, get_lil(start_pos), start_pos); 
            
            case '(': read_char(); return new_token(TokenType::LPAREN,    get_lil(start_pos), start_pos); 
            case ')': read_char(); return new_token(TokenType::RPAREN,    get_lil(start_pos), start_pos); 
            case '{': read_char(); return new_token(TokenType::LBRACE,    get_lil(start_pos), start_pos); 
            case '}': read_char(); return new_token(TokenType::RBRACE,    get_lil(start_pos), start_pos); 
            case '[': read_char(); return new_token(TokenType::LBRACKET,  get_lil(start_pos), start_pos);
            case ']': read_char(); return new_token(TokenType::RBRACKET,  get_lil(start_pos), start_pos);

            case '+': read_char(); return new_token(TokenType::PLUS,      get_lil(start_pos), start_pos); 
            case '*': read_char(); return new_token(TokenType::ASTERISK,  get_lil(start_pos), start_pos); 
            case '/': read_char(); return new_token(TokenType::SLASH,     get_lil(start_pos), start_pos); 
            case '%': read_char(); return new_token(TokenType::MODULUS,   get_lil(start_pos), start_pos); 
            case '^': read_char(); return new_token(TokenType::POW,       get_lil(start_pos), start_pos); 

            case '-':
            {
                if (peek_char() == '>')
                {
                    read_char(); read_char();
                    return new_token(TokenType::ARROW, get_lil(start_pos), start_pos);
                }
                read_char();
                return new_token(TokenType::MINUS, get_lil(start_pos), start_pos);
            }
            case '=':
            {
                if (peek_char() == '=') 
                {
                    read_char(); read_char(); 
                    return new_token(TokenType::EE, get_lil(start_pos), start_pos); 
                }
                if (peek_char() == '>')
                {
                    read_char(); read_char();
                    return new_token(TokenType::FATARROW, get_lil(start_pos), start_pos);
                }
                read_char(); 
                return new_token(TokenType::EQ, get_lil(start_pos), start_pos); 
            }
            case '!':
            {
                if (peek_char() == '=')
                {
                    read_char(); read_char();
                    return new_token(TokenType::NE, get_lil(start_pos), start_pos);
                }
                read_char();
                return new_token(TokenType::BANG, get_lil(start_pos), start_pos);
            }
            case '<':
            {
                if (peek_char() == '=') 
                {
                    read_char(); read_char(); 
                    return new_token(TokenType::LTE, get_lil(start_pos), start_pos); 
                }
                read_char(); 
                return new_token(TokenType::LT, get_lil(start_pos), start_pos); 
            }
            case '>':
            {
                if (peek_char() == '=') 
                {
                    read_char(); read_char(); 
                    return new_token(TokenType::GTE, get_lil(start_pos), start_pos); 
                }
                read_char(); 
                return new_token(TokenType::GT, get_lil(start_pos), start_pos); 
            }
        }

        read_char(); 
        return new_token(TokenType::ILLEGAL, get_lil(start_pos), start_pos); 
    }

    void Lexer::read_char()
    {
        if (read_pos >= source.size()) curr_char = NONE_CHAR;
        else curr_char = source[read_pos];
        pos = read_pos;
        ++read_pos;
    }

    char Lexer::peek_char()
    {   
        if (read_pos >= source.size()) return NONE_CHAR;
        return source[read_pos];
    }

    void Lexer::skip_whitespace()
    {
        while (true) 
        {
            switch (curr_char) 
            {
                case ' ':
                case '\t':
                case '\r': 
                { 
                    read_char(); 
                    break; 
                }
                case '\n': 
                {
                    line++;
                    read_char();
                    line_start_pos = pos;
                    break;
                }
                case '/':
                {
                    if (peek_char() == '/')
                    {
                        while (curr_char != '\n' && curr_char != NONE_CHAR) read_char();
                    }
                    else if (peek_char() == '*')
                    {
                        read_char(); read_char(); 
                        uint32_t depth = 1;
                        while (curr_char != NONE_CHAR && depth > 0)
                        {
                            if (curr_char == '\n')
                            {
                                line++;
                                read_char();
                                line_start_pos = pos;
                            }
                            else if (curr_char == '/' && peek_char() == '*')
                            {
                                depth++;
                                read_char(); read_char();
                            }
                            else if (curr_char == '*' && peek_char() == '/')
                            {
                                depth--;
                                read_char(); read_char();
                            }
                            else read_char();
                        }
                    }
                    else return;
                    break;
                }
                default: return;
            }
        }
    }

    Token Lexer::read_identifier()
    {
        uint32_t start_pos = pos;
        while (curr_char != NONE_CHAR && is_alnum(curr_char)) read_char();
        auto lil = get_lil(start_pos);
        auto type = lookup_ident(lil);
        return new_token(type, lil, start_pos);
    }

    Token Lexer::read_number()
    {
        uint32_t start_pos = pos;
        bool is_decimal = false;
        while (is_digit(curr_char)) read_char();

        if (curr_char == '.' && is_digit(peek_char()))
        {
            is_decimal = true;
            read_char();
            while (is_digit(curr_char)) read_char();
        }

        return new_token(is_decimal ? TokenType::DECIMAL : TokenType::INTEGER, get_lil(start_pos), start_pos);
    }

    Token Lexer::read_string()
    {
        uint32_t start_pos = pos + 1;
        while (true)
        {
            read_char();
            if (curr_char == '\\') read_char();
            else if (curr_char == '"' || curr_char == NONE_CHAR) break;
            else if (curr_char == '\n')
            {
                line++;
                line_start_pos = pos;
            }
        }
        
        auto lil = get_lil(start_pos);
        if (curr_char == '"') read_char(); 
        return new_token(TokenType::STRING, lil, start_pos - 1); 
    }

    Token Lexer::read_label()
    {
        uint32_t start_pos = pos;
        read_char(); 
        if (is_letter(curr_char))
        {
            while (curr_char != NONE_CHAR && is_alnum(curr_char)) read_char();
            return new_token(TokenType::LABEL, get_lil(start_pos), start_pos);
        }
        return new_token(TokenType::ILLEGAL, get_lil(start_pos), start_pos);
    }
}
