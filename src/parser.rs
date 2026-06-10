use crate::lexer::Lexer;
use crate::token::{Token, TokenType};
use crate::ast::*;

#[macro_export]
macro_rules! f {
    ($($arg:tt)*) => { format!($($arg)*) };
}

#[derive(Debug, PartialEq, PartialOrd, Clone, Copy)]
pub enum Precedence
{
    LOWEST = 0,
    EQUALS = 1,
    LESSGREATER = 2,
    SUM = 3,
    PRODUCT = 4,
    EXPONENT = 5,
    PREFIX = 6,
    CALL = 7,
    INDEX = 8
}

impl TokenType<'_>
{
    pub fn precedence(&self) -> Precedence
    {
        match self {
            TokenType::PLUS | TokenType::MINUS => Precedence::SUM,
            TokenType::SLASH | TokenType::ASTERISK | TokenType::MODULUS => Precedence::PRODUCT,
            TokenType::POW => Precedence::EXPONENT,
            TokenType::EE | TokenType::NE => Precedence::EQUALS,
            TokenType::LT | TokenType::GT | TokenType::LTE | TokenType::GTE => Precedence::LESSGREATER,
            TokenType::LPAREN => Precedence::CALL,
            TokenType::LBRACKET => Precedence::INDEX,
            TokenType::PLUSPLUS | TokenType::MINUSMINUS => Precedence::INDEX,
            _ => Precedence::LOWEST,
        }
    }
}

pub struct Parser<'a> 
{
    lexer: Lexer<'a>,
    pub errors: Vec<String>,
    currtok: Token<'a>,
    peektok: Token<'a>,
}

impl<'a> Parser<'a>
{
    pub fn new(mut lexer: Lexer<'a>) -> Self 
    {
        let currtok = lexer.next_token();
        let peektok = lexer.next_token();

        Parser 
        {
            lexer,
            errors: Vec::new(),
            currtok,
            peektok,
        }
    }

    fn next_token(&mut self)
    {
        self.currtok = self.peektok;
        self.peektok = self.lexer.next_token()
    }

    pub fn parse_program(&mut self) -> Program<'a> 
    {
        let mut program = Program { statements: Vec::new() };

        while self.currtok.token_type != TokenType::EOF 
        {
            if let Some(stm) = self.parse_statement() 
            {
                program.statements.push(stm);
            }
            self.next_token();
        }
        program
    }

    fn parse_statement(&mut self) -> Option<Statement<'a>> 
    {
        match self.currtok.token_type 
        {
            TokenType::LET => self.parse_let_statement(),
            TokenType::DEF => self.parse_function_statement(),
            TokenType::RETURN => self.parse_return_statement(),
            TokenType::WHILE => self.parse_while_statement(),
            TokenType::FOR => self.parse_for_statement(),
            TokenType::BREAK => {
                self.next_token(); 
                Some(Statement::Break)
            }
            TokenType::CONTINUE => {
                self.next_token(); 
                Some(Statement::Continue)
            }
            _ => self.parse_expression_statement(),
        }
    }

    fn parse_let_statement(&mut self) -> Option<Statement<'a>> 
    {
        let name = match self.peektok.token_type 
        {
            TokenType::IDENT(lit) => lit,
            _ => { self.peek_error("Identifier"); return None; }
        };
        self.next_token();

        if !self.expect_peek_type(TokenType::COLON) { return None; }
        
        let value_type = match self.peektok.token_type 
        {
            TokenType::TYPE(lit) => lit,
            _ => { self.peek_error("Type"); return None; }
        };
        self.next_token();

        let mut array_size = None;
        if self.peektok.token_type == TokenType::LBRACKET 
        {
            self.next_token(); 
            self.next_token(); 
            array_size = Some(self.parse_expression(Precedence::LOWEST)?);
            if !self.expect_peek_type(TokenType::RBRACKET) { return None; }
        }

        if !self.expect_peek_type(TokenType::EQ) { return None; }
        self.next_token();

        let value = self.parse_expression(Precedence::LOWEST)?;

        while self.currtok.token_type != TokenType::SEMICOLON && self.currtok.token_type != TokenType::EOF {
            self.next_token();
        }

        Some(Statement::Let(LetStatement { name, value_type, array_size, value }))
    }

    fn parse_function_statement(&mut self) -> Option<Statement<'a>> 
    {
        let name = match self.peektok.token_type 
        {
            TokenType::IDENT(lit) => lit,
            _ => { self.peek_error("Function Name"); return None; }
        };
        self.next_token();

        if !self.expect_peek_type(TokenType::LPAREN) { return None; }
        let parameters = self.parse_function_parameters()?;

        if !self.expect_peek_type(TokenType::ARROW) { return None; }
        
        let return_type = match self.peektok.token_type {
            TokenType::TYPE(lit) => lit,
            _ => { self.peek_error("Return Type"); return None; }
        };
        self.next_token();

        if !self.expect_peek_type(TokenType::LBRACE) { return None; }
        let body = self.parse_block_statement();

        Some(Statement::Function(FunctionStatement { name, parameters, return_type, body }))
    }

    fn parse_function_parameters(&mut self) -> Option<Vec<FunctionParameter<'a>>> 
    {
        let mut params = Vec::new();
        if self.peektok.token_type == TokenType::RPAREN {
            self.next_token();
            return Some(params);
        }

        self.next_token();
        let name = match self.currtok.token_type 
        {
            TokenType::IDENT(lit) => lit,
            _ => return None,
        };

        if !self.expect_peek_type(TokenType::COLON) { return None; }
        let value_type = match self.peektok.token_type {
            TokenType::TYPE(lit) => lit,
            _ => return None,
        };
        self.next_token();
        params.push(FunctionParameter { name, value_type });

        while self.peektok.token_type == TokenType::COMMA {
            self.next_token(); 
            self.next_token(); 
            
            let p_name = match self.currtok.token_type {
                TokenType::IDENT(lit) => lit,
                _ => return None,
            };
            if !self.expect_peek_type(TokenType::COLON) { return None; }
            let p_type = match self.peektok.token_type {
                TokenType::TYPE(lit) => lit,
                _ => return None,
            };
            self.next_token();
            params.push(FunctionParameter { name: p_name, value_type: p_type });
        }

        if !self.expect_peek_type(TokenType::RPAREN) { return None; }
        Some(params)
    }

    fn parse_return_statement(&mut self) -> Option<Statement<'a>> 
    {
        self.next_token();
        if self.currtok.token_type == TokenType::SEMICOLON {
            return Some(Statement::Return(ReturnStatement { return_value: Expression::Void }));
        }
        let return_value = self.parse_expression(Precedence::LOWEST)?;
        if !self.expect_peek_type(TokenType::SEMICOLON) { return None; }
        Some(Statement::Return(ReturnStatement { return_value }))
    }

    fn parse_while_statement(&mut self) -> Option<Statement<'a>> 
    {
        self.next_token();
        let condition = self.parse_expression(Precedence::LOWEST)?;
        if !self.expect_peek_type(TokenType::LBRACE) { return None; }
        let body = self.parse_block_statement();
        Some(Statement::While(WhileStatement { condition, body }))
    }

    fn parse_for_statement(&mut self) -> Option<Statement<'a>> 
    {
        if !self.expect_peek_type(TokenType::LET) { return None; }
        
        let var_declaration = match self.parse_let_statement()? 
        {
            Statement::Let(s) => s,
            _ => return None,
        };

        self.next_token();
        let condition = self.parse_expression(Precedence::LOWEST)?;
        if !self.expect_peek_type(TokenType::SEMICOLON) { return None; }
        
        self.next_token();
        let action = self.parse_expression(Precedence::LOWEST)?;

        if !self.expect_peek_type(TokenType::LBRACE) { return None; }
        let body = self.parse_block_statement();

        Some(Statement::For(ForStatement { var_declaration, condition, action, body }))
    }

    fn parse_block_statement(&mut self) -> BlockStatement<'a> 
    {
        let mut statements = Vec::new();
        self.next_token();

        while self.currtok.token_type != TokenType::RBRACE && self.currtok.token_type != TokenType::EOF 
        {
            if let Some(stm) = self.parse_statement() 
            {
                statements.push(stm);
            }
            self.next_token();
        }
        BlockStatement { statements }
    }

    fn parse_expression_statement(&mut self) -> Option<Statement<'a>> 
    {
        let left_expr = self.parse_expression(Precedence::LOWEST)?;

        if self.peek_token_is_assignment() 
        {
            self.next_token();
            let operator = self.currtok.token_type.get_literal();
            self.next_token();
            let rvalue = self.parse_expression(Precedence::LOWEST)?;
            
            if self.peektok.token_type == TokenType::SEMICOLON { self.next_token(); }
            return Some(Statement::Assign(AssignStatement { ident: left_expr, operator, rvalue }));
        }

        if self.peektok.token_type == TokenType::SEMICOLON { self.next_token(); }
        Some(Statement::Expression(left_expr))
    }

    fn parse_expression(&mut self, precedence: Precedence) -> Option<Expression<'a>> 
    {
        let mut left_expr = match self.currtok.token_type 
        {
            TokenType::INT(val) => Expression::Integer(val),
            TokenType::FLOAT(val) => Expression::Float(val),
            TokenType::TRUE => Expression::Boolean(true),
            TokenType::FALSE => Expression::Boolean(false),
            TokenType::IDENT(lit) => Expression::Identifier(lit),
            TokenType::STRING(lit) => Expression::String(lit),
            TokenType::MINUS | TokenType::PLUS | TokenType::BANG => self.parse_prefix_expr()?,
            TokenType::LPAREN => self.parse_grouped_expr()?,
            TokenType::LBRACKET => self.parse_array_literal()?,
            TokenType::IF => self.parse_if_expr()?,
            _ => {
                self.errors.push(f!("No prefix function for {:?} found", self.currtok.token_type));
                return None;
            }
        };

        while self.peektok.token_type != TokenType::SEMICOLON && precedence < self.peektok.token_type.precedence() 
        {
            match self.peektok.token_type 
            {
                TokenType::PLUS | TokenType::MINUS | TokenType::SLASH | TokenType::ASTERISK |
                TokenType::MODULUS | TokenType::POW | TokenType::EE | TokenType::NE |
                TokenType::LT | TokenType::GT | TokenType::LTE | TokenType::GTE => {
                    self.next_token();
                    left_expr = self.parse_infix_expr(left_expr)?;
                }
                TokenType::LPAREN => {
                    self.next_token();
                    left_expr = self.parse_call_expr(left_expr)?;
                }
                TokenType::LBRACKET => {
                    self.next_token();
                    left_expr = self.parse_index_expr(left_expr)?;
                }
                TokenType::PLUSPLUS | TokenType::MINUSMINUS => {
                    self.next_token();
                    left_expr = self.parse_postfix_expr(left_expr)?;
                }
                _ => return Some(left_expr),
            }
        }

        Some(left_expr)
    }

    fn parse_prefix_expr(&mut self) -> Option<Expression<'a>> 
    {
        let operator = self.currtok.token_type.get_literal();
        self.next_token();
        let right = self.parse_expression(Precedence::PREFIX)?;
        Some(Expression::Prefix(Box::new(PrefixExpression { operator, right })))
    }

    fn parse_infix_expr(&mut self, left: Expression<'a>) -> Option<Expression<'a>> 
    {
        let operator = self.currtok.token_type.get_literal();
        let precedence = self.currtok.token_type.precedence();
        self.next_token();
        let right = self.parse_expression(precedence)?;
        Some(Expression::Infix(Box::new(InfixExpression { left, operator, right })))
    }

    fn parse_postfix_expr(&mut self, left: Expression<'a>) -> Option<Expression<'a>> 
    {
        let operator = self.currtok.token_type.get_literal();
        Some(Expression::Postfix(Box::new(PostfixExpression { operator, left })))
    }

    fn parse_grouped_expr(&mut self) -> Option<Expression<'a>> 
    {
        self.next_token();
        let expr = self.parse_expression(Precedence::LOWEST)?;
        if !self.expect_peek_type(TokenType::RPAREN) { return None; }
        Some(expr)
    }

    fn parse_index_expr(&mut self, left: Expression<'a>) -> Option<Expression<'a>> 
    {
        self.next_token();
        let index = self.parse_expression(Precedence::LOWEST)?;
        if !self.expect_peek_type(TokenType::RBRACKET) { return None; }
        Some(Expression::Index(Box::new(IndexExpression { left, index })))
    }

    fn parse_call_expr(&mut self, function: Expression<'a>) -> Option<Expression<'a>> 
    {
        let arguments = self.parse_expression_list(TokenType::RPAREN)?;
        Some(Expression::Call(Box::new(CallExpression { function, arguments })))
    }

    fn parse_if_expr(&mut self) -> Option<Expression<'a>> 
    {
        let mut branches = Vec::new();
        let mut alternative = None;

        self.next_token(); 
        let first_condition = self.parse_expression(Precedence::LOWEST)?;
        if !self.expect_peek_type(TokenType::LBRACE) {
            return None;
        }
        let first_consequence = self.parse_block_statement();
        branches.push(IfBranch {
            condition: first_condition,
            consequence: first_consequence,
        });
        while self.peektok.token_type == TokenType::ELSE {
            self.next_token(); 

            if self.peektok.token_type == TokenType::IF 
            {
                self.next_token(); 
                self.next_token(); 

                let next_condition = self.parse_expression(Precedence::LOWEST)?;
                if !self.expect_peek_type(TokenType::LBRACE) {
                    return None;
                }
                let next_consequence = self.parse_block_statement();

                branches.push(IfBranch {
                    condition: next_condition,
                    consequence: next_consequence,
                });
            } 
            else 
            {
                if !self.expect_peek_type(TokenType::LBRACE) 
                {
                    return None;
                }
                alternative = Some(self.parse_block_statement());
                break;
            }
        }
        Some(Expression::If(Box::new(IfExpression {
            branches,
            alternative,
        })))
    }

    fn parse_expression_list(&mut self, end: TokenType<'a>) -> Option<Vec<Expression<'a>>> 
    {
        let mut list = Vec::new();
        if self.peektok.token_type == end 
        {
            self.next_token();
            return Some(list);
        }

        self.next_token();
        list.push(self.parse_expression(Precedence::LOWEST)?);

        while self.peektok.token_type == TokenType::COMMA 
        {
            self.next_token();
            self.next_token();
            list.push(self.parse_expression(Precedence::LOWEST)?);
        }

        if !self.expect_peek_type(end) { return None; }
        Some(list)
    }

    fn parse_array_literal(&mut self) -> Option<Expression<'a>> 
    {
        let elements = self.parse_expression_list(TokenType::RBRACKET)?;
        let size = elements.len();
        Some(Expression::Array(ArrayLiteral { elements, size }))
    }

    fn expect_peek_type(&mut self, tt: TokenType<'a>) -> bool 
    {
        if std::mem::discriminant(&self.peektok.token_type) == std::mem::discriminant(&tt) 
        {
            self.next_token();
            true
        } 
        else 
        {
            let line = self.peektok.line_no;
            let pos = self.peektok.position;

            self.errors.push(format!(
                "Invalid Synctax [line {}, col {}]: Expected next token to be {:?}, but got {:?}", 
                line, pos, tt, self.peektok.token_type
            ));
            false
        }
    }

    fn peek_token_is_assignment(&self) -> bool 
    {
        matches!(
            self.peektok.token_type,
            TokenType::EQ | TokenType::PLUSEQ | TokenType::MINUSEQ | TokenType::MULEQ | TokenType::DIVEQ | TokenType::MODEQ
        )
    }

    fn peek_error(&mut self, expected: &str)
    {
        self.errors.push(format!("Expected next token to be {}, got {:?}", expected, self.peektok.token_type));
    }

}