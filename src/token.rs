#[derive(Debug, PartialEq, Clone, Copy)]
pub enum TokenType<'a>
{
    EOF,
    ILLEGAL(&'a str),
    IDENT(&'a str), INT(i32), FLOAT(f32), STRING(&'a str),
    PLUS, MINUS, ASTERISK, SLASH, MODULUS, POW, 
    EQ, PLUSEQ, MINUSEQ, MULEQ, DIVEQ, MODEQ,
    PLUSPLUS, MINUSMINUS,

    LBRACKET, RBRACKET, LPAREN, RPAREN, LBRACE, RBRACE, 
    COLON, SEMICOLON, ARROW, COMMA, BANG,

    LT, LTE, GT, GTE, EE, NE,

    LET, DEF, RETURN, IF, ELSE, TRUE, FALSE, WHILE, CONTINUE, BREAK, FOR,
    TYPE(&'a str)
}

#[derive(Debug, Clone, Copy)]
pub struct Token<'a> 
{
    pub token_type: TokenType<'a>, 
    pub line_no: usize,
    pub position: usize,
}

impl<'a> TokenType<'a>
{
    pub fn lookup_ident(ident: &'a str) -> TokenType<'a>
    {
        match ident
        {
            "let" => TokenType::LET,
            "def" => TokenType::DEF,
            "return" => TokenType::RETURN,
            "if" => TokenType::IF,
            "else" => TokenType::ELSE,
            "true" => TokenType::TRUE,
            "false" => TokenType::FALSE,
            "while" => TokenType::WHILE,
            "for" => TokenType::FOR,
            "continue" => TokenType::CONTINUE,
            "break" => TokenType::BREAK,
            "int" | "float" | "str" | "bool" | "void" => TokenType::TYPE(ident),
            _ => {
                if ident.ends_with("[]")
                {
                    let base = &ident[..ident.len() - 2];
                    match base {
                        "int" | "float" | "str" | "bool" | "void" => {
                            return TokenType::TYPE(ident);
                        }
                        _ => {}
                    }
                }
                TokenType::IDENT(ident)
            }
        }
    }

    pub fn get_literal(&self) -> &'a str {
    match self {
        TokenType::PLUS => "+",
        TokenType::MINUS => "-",
        TokenType::ASTERISK => "*",
        TokenType::SLASH => "/",
        TokenType::MODULUS => "%",
        TokenType::POW => "^",
        TokenType::BANG => "!",
        TokenType::EQ => "=",
        TokenType::PLUSEQ => "+=",
        TokenType::MINUSEQ => "-=",
        TokenType::MULEQ => "*=",
        TokenType::DIVEQ => "/=",
        TokenType::MODEQ => "%=",
        TokenType::PLUSPLUS => "++",
        TokenType::MINUSMINUS => "--",
        TokenType::LT => "<",
        TokenType::LTE => "<=",
        TokenType::GT => ">",
        TokenType::GTE => ">=",
        TokenType::EE => "==",
        TokenType::NE => "!=",
        TokenType::IDENT(lit) => lit,
        TokenType::TYPE(lit) => lit,
        _ => "",
    }
}
}