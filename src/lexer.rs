use crate::token::{TokenType, Token};

pub struct Lexer<'a>
{
    source: &'a str,
    pos: usize,
    read_pos: usize,
    line_no: usize,
    curr_char: Option<char>
}

impl<'a> Lexer<'a>
{
    pub fn new(source: &'a str) -> Self
    {
        let mut l = Lexer
        {
            source, pos: 0,read_pos: 0, line_no: 1, curr_char: None
        };
        l.read_char();
        l
    }

    fn read_char(&mut self)
    {
        if self.read_pos >= self.source.len()
        {
            self.curr_char = None
        }
        else 
        {
            self.curr_char = self.source[self.read_pos..].chars().next();
            self.pos = self.read_pos;
            if let Some(ch) = self.curr_char 
            {
                self.read_pos += ch.len_utf8(); 
            }
        }
    }

    fn peek_char(&self) -> Option<char>
    {
        if self.read_pos >= self.source.len()
        {
            None
        }
        else 
        {
            self.source[self.read_pos..].chars().next()
        }
    }

    fn skip_whitespace(&mut self)
    {
        while let Some(ch) = self.curr_char
        {
            if ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r'
            {
                if ch == '\n'
                {
                    self.line_no += 1;
                }
                self.read_char();
            }
            else
            {
                break;
            }
        }
    }

    fn new_token(&self, tt: TokenType<'a>) -> Token<'a>
    {
        Token
        {
            token_type: tt,
            line_no: self.line_no,
            position: self.pos
        }
    }

    fn read_string(&mut self) -> &'a str
    {
        let start_pos = self.pos + 1;
        loop 
        {
            self.read_char();
            match self.curr_char 
            {
                Some('"') | None => break,
                _ => {}
            }
        }
        &self.source[start_pos..self.pos]
    }

    fn read_number(&mut self) -> Token<'a>
    {
        let start_pos = self.pos;
        let mut dot_cnt = 0;

        while let Some(ch) = self.curr_char
        {
            if ch.is_ascii_digit() || ch == '.'
            {
                if ch == '.'
                {
                    dot_cnt += 1;
                }
                if dot_cnt > 1
                {
                    let illegal_slice = &self.source[start_pos..=self.pos];
                    self.read_char();
                    return self.new_token(TokenType::ILLEGAL(illegal_slice));
                }
                self.read_char();
            }
            else 
            {
                break;
            }
        }

        let num_slice = &self.source[start_pos..self.pos];
        if dot_cnt == 0 
        {
            let val = num_slice.parse::<i32>().unwrap_or(0);
            self.new_token(TokenType::INT(val))
        } 
        else 
        {
            let val = num_slice.parse::<f32>().unwrap_or(0.0);
            self.new_token(TokenType::FLOAT(val))
        }
    }

    pub fn read_identifier(&mut self) -> &'a str
    {
        let start_pos = self.pos;
        while let Some(ch) = self.curr_char 
        {
            if ch.is_alphanumeric() || ch == '_' 
            {
                self.read_char();
            } 
            else 
            {
                break;
            }
        }
        &self.source[start_pos..self.pos]
    }

    pub fn next_token(&mut self) -> Token<'a> {
        self.skip_whitespace();

        let tok = match self.curr_char {
            Some('+') => {
                if self.peek_char() == Some('=') 
                {
                    self.read_char();
                    self.new_token(TokenType::PLUSEQ)
                } 
                else if self.peek_char() == Some('+') 
                {
                    self.read_char();
                    self.new_token(TokenType::PLUSPLUS)
                } 
                else 
                {
                    self.new_token(TokenType::PLUS)
                }
            }
            Some('-') => {
                if self.peek_char() == Some('>') 
                {
                    self.read_char();
                    self.new_token(TokenType::ARROW)
                } 
                else if self.peek_char() == Some('=') 
                {
                    self.read_char();
                    self.new_token(TokenType::MINUSEQ)
                } 
                else if self.peek_char() == Some('-') 
                {
                    self.read_char();
                    self.new_token(TokenType::MINUSMINUS)
                } 
                else 
                {
                    self.new_token(TokenType::MINUS)
                }
            }
            Some('*') => {
                if self.peek_char() == Some('*') 
                {
                    self.read_char();
                    self.new_token(TokenType::POW)
                } 
                else if self.peek_char() == Some('=') 
                {
                    self.read_char();
                    self.new_token(TokenType::MULEQ)
                } 
                else 
                {
                    self.new_token(TokenType::ASTERISK)
                }
            }
            Some('/') => {
                if self.peek_char() == Some('/') 
                {
                    while self.curr_char != Some('\n') && self.curr_char.is_some() 
                    {
                        self.read_char();
                    }
                    return self.next_token(); 
                } 
                else if self.peek_char() == Some('=') 
                {
                    self.read_char();
                    self.new_token(TokenType::DIVEQ)
                } 
                else 
                {
                    self.new_token(TokenType::SLASH)
                }
            }
            Some('%') => {
                if self.peek_char() == Some('=')
                {
                    self.read_char();
                    self.new_token(TokenType::MODEQ)
                } 
                else 
                {
                    self.new_token(TokenType::MODULUS)
                }
            }
            Some('=') => {
                if self.peek_char() == Some('=') 
                {
                    self.read_char();
                    self.new_token(TokenType::EE)
                } 
                else 
                {
                    self.new_token(TokenType::EQ)
                }
            }
            Some('!') => {
                if self.peek_char() == Some('=') 
                {
                    self.read_char();
                    self.new_token(TokenType::NE)
                } 
                else 
                {
                    self.new_token(TokenType::BANG)
                }
            }
            Some('<') => {
                if self.peek_char() == Some('=') 
                {
                    self.read_char();
                    self.new_token(TokenType::LTE)
                } 
                else 
                {
                    self.new_token(TokenType::LT)
                }
            }
            Some('>') => {
                if self.peek_char() == Some('=') 
                {
                    self.read_char();
                    self.new_token(TokenType::GTE)
                } 
                else 
                {
                    self.new_token(TokenType::GT)
                }
            }
            Some('"') => {
                let lit = self.read_string();
                self.new_token(TokenType::STRING(lit))
            }
            Some('[') => self.new_token(TokenType::LBRACKET),
            Some(']') => self.new_token(TokenType::RBRACKET),
            Some('(') => self.new_token(TokenType::LPAREN),
            Some(')') => self.new_token(TokenType::RPAREN),
            Some('{') => self.new_token(TokenType::LBRACE),
            Some('}') => self.new_token(TokenType::RBRACE),
            Some(':') => self.new_token(TokenType::COLON),
            Some(';') => self.new_token(TokenType::SEMICOLON),
            Some(',') => self.new_token(TokenType::COMMA),
            None => self.new_token(TokenType::EOF),
            Some(ch) => {
                if ch.is_alphabetic() || ch == '_' {
                    let ident = self.read_identifier();
                    let tt = TokenType::lookup_ident(ident);
                    return Token 
                    {
                        token_type: tt,
                        line_no: self.line_no,
                        position: self.pos,
                    }; 
                } else if ch.is_ascii_digit() 
                {
                    return self.read_number();
                } 
                else 
                {
                    let illegal_slice = &self.source[self.pos..self.read_pos];
                    self.new_token(TokenType::ILLEGAL(illegal_slice))
                }
            }
        };

        self.read_char();
        tok
    }
}
