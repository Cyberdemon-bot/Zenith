use serde::Serialize;


#[derive(Debug, Clone, Serialize, PartialEq)]
pub enum Statement<'a>
{
    Let(LetStatement<'a>),
    Block(BlockStatement<'a>),
    Return(ReturnStatement<'a>),
    Function(FunctionStatement<'a>),
    Assign(AssignStatement<'a>),
    While(WhileStatement<'a>),
    For(ForStatement<'a>),
    Break,
    Continue,
    Expression(Expression<'a>),
}

#[derive(Debug, Clone, Serialize, PartialEq)]
pub enum Expression<'a>
{
    Identifier(&'a str),
    Integer(i32),
    Float(f32),
    Boolean(bool),
    String(&'a str),
    Void,
    Array(ArrayLiteral<'a>),
    Index(Box<IndexExpression<'a>>),
    Prefix(Box<PrefixExpression<'a>>),
    Infix(Box<InfixExpression<'a>>),
    Postfix(Box<PostfixExpression<'a>>),
    If(Box<IfExpression<'a>>),
    Call(Box<CallExpression<'a>>),
}

#[derive(Debug, Clone, Serialize, PartialEq)]
pub struct LetStatement<'a>
{
    pub name: &'a str,
    pub value_type: &'a str,
    pub array_size: Option<Vec<Expression<'a>>>,
    pub value: Expression<'a>
}

#[derive(Debug, Clone, Serialize, PartialEq)]
pub struct BlockStatement<'a> 
{
    pub statements: Vec<Statement<'a>>,
}

#[derive(Debug, Clone, Serialize, PartialEq)]
pub struct ReturnStatement<'a> 
{
    pub return_value: Expression<'a>,
}

#[derive(Debug, Clone, Serialize, PartialEq)]
pub struct FunctionParameter<'a> 
{
    pub name: &'a str,
    pub value_type: &'a str,
    pub array_size: Option<Vec<Expression<'a>>>,
}

#[derive(Debug, Clone, Serialize, PartialEq)]
pub struct FunctionStatement<'a> 
{
    pub name: &'a str,
    pub parameters: Vec<FunctionParameter<'a>>,
    pub return_type: &'a str,
    pub body: BlockStatement<'a>,
}

#[derive(Debug, Clone, Serialize, PartialEq)]
pub struct AssignStatement<'a> 
{
    pub ident: Expression<'a>,
    pub operator: &'a str,
    pub rvalue: Expression<'a>,
}

#[derive(Debug, Clone, Serialize, PartialEq)]
pub struct WhileStatement<'a> 
{
    pub condition: Expression<'a>,
    pub body: BlockStatement<'a>,
}

#[derive(Debug, Clone, Serialize, PartialEq)]
pub struct ForStatement<'a> 
{
    pub var_declaration: LetStatement<'a>,
    pub condition: Expression<'a>,
    pub action: Expression<'a>, 
    pub body: BlockStatement<'a>,
}

#[derive(Debug, Clone, Serialize, PartialEq)]
pub struct ArrayLiteral<'a> 
{
    pub elements: Vec<Expression<'a>>,
    pub size: usize,
}

#[derive(Debug, Clone, Serialize, PartialEq)]
pub struct IndexExpression<'a> 
{
    pub left: Expression<'a>,
    pub index: Expression<'a>,
}

#[derive(Debug, Clone, Serialize, PartialEq)]
pub struct PrefixExpression<'a> 
{
    pub operator: &'a str,
    pub right: Expression<'a>,
}

#[derive(Debug, Clone, Serialize, PartialEq)]
pub struct InfixExpression<'a> 
{
    pub left: Expression<'a>,
    pub operator: &'a str,
    pub right: Expression<'a>,
}

#[derive(Debug, Clone, Serialize, PartialEq)]
pub struct PostfixExpression<'a> 
{
    pub operator: &'a str,
    pub left: Expression<'a>,
}

#[derive(Debug, Clone, Serialize, PartialEq)]
pub struct IfBranch<'a> 
{
    pub condition: Expression<'a>,
    pub consequence: BlockStatement<'a>,
}

#[derive(Debug, Clone, Serialize, PartialEq)]
pub struct IfExpression<'a> 
{
    pub branches: Vec<IfBranch<'a>>,
    pub alternative: Option<BlockStatement<'a>>,
}

#[derive(Debug, Clone, Serialize, PartialEq)]
pub struct CallExpression<'a> 
{
    pub function: Expression<'a>,
    pub arguments: Vec<Expression<'a>>,
}

#[derive(Debug, Clone, Serialize, PartialEq)]
pub struct Program<'a> 
{
    pub statements: Vec<Statement<'a>>,
}
