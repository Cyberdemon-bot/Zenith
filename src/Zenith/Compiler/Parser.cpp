#include <charconv>
#include "Zenith/Core/Base.h"
#include "Zenith/Compiler/Parser.h"

namespace Zenith {

    namespace {
        constexpr Operator TokenToBinaryOp(TokenType type) noexcept
        {
            switch (type)
            {
                case TokenType::PLUS:     return Operator::Add;
                case TokenType::MINUS:    return Operator::Sub;
                case TokenType::ASTERISK: return Operator::Mul;
                case TokenType::SLASH:    return Operator::Div;
                case TokenType::MODULUS:  return Operator::Mod;
                case TokenType::EE:       return Operator::Eq;
                case TokenType::NE:       return Operator::Ne;
                case TokenType::LT:       return Operator::Lt;
                case TokenType::LTE:      return Operator::Lte;
                case TokenType::GT:       return Operator::Gt;
                case TokenType::GTE:      return Operator::Gte;
                case TokenType::AND:      return Operator::And;
                case TokenType::OR:       return Operator::Or;
                default:                  return Operator::Add;
            }
        }
    }

    Parser::Parser(std::string_view source)
         : m_Source(source), m_Lexer(source)
    {
        m_Tokens.reserve(256);
        m_Nodes.push_back({});
        Token token;
        do 
        {
            token = m_Lexer.NextToken();
            m_Tokens.push_back(token);
        } while (token.type != TokenType::END_OF_FILE);
    }

    AstView Parser::ParseProgram()
    {
        size_t marker = GetScratchMarker();
        uint32_t start = CurrToken(), end;

        while (!CurrTokenIs(TokenType::END_OF_FILE))
        {
            if (CurrTokenIs(TokenType::RBRACE))
            {
                AddError("Unexpected '}' at top level");
                NextToken();
                continue;
            }

            uint32_t stmt = ParseStatement();
            if (stmt == ERROR_NODE) Synchronize();
            else PushScratch(stmt);
        }

        end = CurrToken();
        auto list = CommitList(marker);
        auto root = PushNode(NodeType::Program, start, end, { .list = { .start = list.start, .count = list.count } });
        return AstView
        {
            .source = m_Source,
            .nodes = m_Nodes,
            .tokens = m_Tokens,
            .extra = m_Extra,
            .root = root
        };
    }

    // expression parsing
    uint32_t Parser::ParseExpr(PrecedenceType precedence)
    {
        DepthGuard guard(m_Depth);
        if (m_Depth > MAX_DEPTH) return AddError("Nesting is too deep");
        uint32_t left = ParsePrefixExpr();
        if (left == ERROR_NODE) return ERROR_NODE;
        while (precedence < GetPrecedence(GetCurrToken().type))
        {
            left = ParseInfixExpr(left);
            if (left == ERROR_NODE) return ERROR_NODE;
        }
        return left;
    }

    uint32_t Parser::ParsePrefixExpr()
    {
        const auto& curr = GetCurrToken();
        if (IsBlockLikeStart(GetCurrToken().type)) return ParseBlockLikeExpr();
        switch (curr.type) 
        {
            case TokenType::INTEGER: 
            {
                Token tok = GetCurrToken();
                int64_t val = 0;
                auto [ptr, ec] = std::from_chars(tok.literal.data(), tok.literal.data() + tok.literal.size(), val);
                if (ec != std::errc{}) return AddError("Integer literal out of range");
                uint32_t idx = PushNode(NodeType::Integer, CurrToken(), { .i64 = val });
                NextToken(); 
                return idx;
            }
            case TokenType::DECIMAL: 
            {
                Token tok = GetCurrToken();
                double val = 0.0;
                auto [ptr, ec] = std::from_chars(tok.literal.data(), tok.literal.data() + tok.literal.size(), val);
                if (ec != std::errc{}) return AddError("Invalid decimal literal");
                uint32_t idx = PushNode(NodeType::Decimal, CurrToken(), { .f64 = val });
                NextToken(); 
                return idx;
            }
            case TokenType::STRING: 
            {
                uint32_t idx = PushNode(NodeType::String, CurrToken()); 
                NextToken(); 
                return idx;
            }
            case TokenType::TRUE:
            {
                uint32_t idx = PushNode(NodeType::Bool, CurrToken(), { .i64 = 1 });
                NextToken();
                return idx;
            }
            case TokenType::FALSE:
            {
                uint32_t idx = PushNode(NodeType::Bool, CurrToken(), { .i64 = 0 });
                NextToken();
                return idx;
            }
            case TokenType::NULL_LIT:
            {
                uint32_t idx = PushNode(NodeType::Null, CurrToken());
                NextToken();
                return idx;
            }
            case TokenType::BANG:
            case TokenType::MINUS: 
            {
                uint32_t op_tok = CurrToken();
                Operator op = (curr.type == TokenType::BANG) ? Operator::Not : Operator::Neg;
                NextToken(); 
                uint32_t expr = ParseExpr(PrecedenceType::P_UNARY);
                if (expr == ERROR_NODE) return ERROR_NODE;
                return PushNode(NodeType::UnaryExpr, op_tok, GetNode(expr).end_token,
                    { .unary = { .op = op, .operand = expr } });
            }
            case TokenType::ASTERISK:
            {
                uint32_t op_tok = CurrToken();
                NextToken();
                uint32_t expr = ParseExpr(PrecedenceType::P_UNARY);
                if (expr == ERROR_NODE) return ERROR_NODE;
                uint32_t end = GetNode(expr).end_token;
                uint32_t node = PushNode(NodeType::DerefExpr, op_tok, end, { .child = expr });
                return node;
            }
            case TokenType::LPAREN: return ParseParenExpr();
            case TokenType::IDENT:
            {
                if (PeekTokenIs(TokenType::LBRACE) && !m_NoStructLit && IsStructInitAhead())
                    return ParseStructInit();
                return ParseIdentifier();
            }
            case TokenType::AMPERSAND: return ParseBorrowExpr(CurrToken());
            case TokenType::MUT: return ParseMutExpr();
            case TokenType::NEW: return ParseNewExpr(CurrToken(), 0);
            default: return AddError("Unexpected token in prefix expression");
        }
    }

    uint32_t Parser::ParseInfixExpr(uint32_t left)
    {
        const auto& op_token = GetCurrToken();

        switch (op_token.type)
        {
            case TokenType::PLUS:
            case TokenType::MINUS:
            case TokenType::ASTERISK:
            case TokenType::SLASH:
            case TokenType::MODULUS:
            case TokenType::EE:
            case TokenType::NE:
            case TokenType::LT:
            case TokenType::GT:
            case TokenType::LTE:
            case TokenType::GTE:
            case TokenType::AND:
            case TokenType::OR: return ParseNumericExpr(left);
            case TokenType::AS: return ParseCastExpr(left);
            case TokenType::DOT: return ParseFieldAccessExpr(left);
            case TokenType::LPAREN:  return ParseCallExpr(left);
            case TokenType::LBRACKET: return ParseIndexExpr(left);
            default: return AddError("Unexpected token in binary/infix expression", left);
        }
    }

    uint32_t Parser::ParseBlockLikeExpr()
    {
        switch (GetCurrToken().type)
        {
            case TokenType::LBRACE: return ParseBlockExpr();
            case TokenType::IF:     return ParseIfExpr();
            case TokenType::FOR:    return ParseForExpr();
            case TokenType::LABEL:  return ParseLabeledExpr();
            default:                return ERROR_NODE; 
        }
    }

    uint32_t Parser::ParseLabeledExpr()
    {
        uint32_t start = CurrToken(), end; NextToken();
        if (!CurrTokenIs(TokenType::COLON)) 
            return AddError("Expected ':' after label");
        NextToken();

        if (CurrTokenIs(TokenType::LABEL) || !IsBlockLikeStart(GetCurrToken().type))
            return AddError("A label must be followed by a block, 'if', or 'for'");
        uint32_t body = ParseBlockLikeExpr();
        if (body == ERROR_NODE) return ERROR_NODE;

        end = GetNode(body).end_token;
        uint32_t label = PushNode(NodeType::Label, start);
        return PushNode(NodeType::LabeledExpr, start, end,
            { .labeled = { .label = label, .body = body } });
    }

    uint32_t Parser::ParseNumericExpr(uint32_t left)
    {
        const auto& token = GetCurrToken();
        Operator op = TokenToBinaryOp(token.type);
        NextToken();
        PrecedenceType precedence = GetPrecedence(token.type);
        
        uint32_t right = ParseExpr(precedence);
        if (right == ERROR_NODE) return ERROR_NODE;

        uint32_t start = GetNode(left).start_token, end = GetNode(right).end_token;
        return PushNode(NodeType::BinaryExpr, start, end,
            { .binary = { .lhs = left, .op = op, .rhs = right } });
    }

    uint32_t Parser::ParseCastExpr(uint32_t left)
    {
        NextToken();
        uint32_t targetType = ParseTypeAnnotation();
        if (targetType == ERROR_NODE) return ERROR_NODE;

        uint32_t start = GetNode(left).start_token, end = GetNode(targetType).end_token;
        return PushNode(NodeType::CastExpr, start, end,
            { .cast = { .expr = left, .type = targetType } });
    }

    uint32_t Parser::ParseFieldAccessExpr(uint32_t left)
    {
        NextToken(); // consume '.'
        if (!CurrTokenIs(TokenType::IDENT)) return AddError("Expected field identifier after '.'", left);
        uint32_t field = ParseIdentifier();
        uint32_t start = GetNode(left).start_token, end = GetNode(field).end_token;
        return PushNode(NodeType::FieldAccess, start, end,
            { .access = { .target = left, .member = field } });
    }

    uint32_t Parser::ParseCallExpr(uint32_t left)
    {
        StructLitScope allow(m_NoStructLit, false);
        uint32_t args = ParseDelimitedList(
            NodeType::ArgList,
            TokenType::RPAREN,
            TokenType::COMMA,
            [this]() { return ParseExpr(PrecedenceType::P_LOWEST); },
            "Expected ',' or ')' in function call argument list"
        );
        if (args == ERROR_NODE) return ERROR_NODE;

        uint32_t start = GetNode(left).start_token, end = GetNode(args).end_token;
        return PushNode(NodeType::CallExpr, start, end,
            { .call = { .callee = left, .args = args } });
    }

    uint32_t Parser::ParseIndexExpr(uint32_t left)
    {
        StructLitScope allow(m_NoStructLit, false);
        NextToken(); // consume '['
        uint32_t index_expr = ParseExpr(PrecedenceType::P_LOWEST);
        if (index_expr == ERROR_NODE) return ERROR_NODE;
        if (!CurrTokenIs(TokenType::RBRACKET)) return AddError("Expected ']' after index expression", index_expr); 
        
        uint32_t start = GetNode(left).start_token, end = CurrToken();
        NextToken(); // consume ']'

        return PushNode(NodeType::IndexExpr, start, end,
            { .access = { .target = left, .member = index_expr } });
    }

    uint32_t Parser::ParseBorrowExpr(uint32_t start)
    {
        NextToken(); // consume '&'
        uint8_t is_mut = 0;
        if (CurrTokenIs(TokenType::MUT))
        {
            is_mut = 1;
            NextToken(); // consume 'mut'
        }

        uint32_t expr = ParseExpr(PrecedenceType::P_UNARY);
        if (expr == ERROR_NODE) return ERROR_NODE;

        return PushNode(NodeType::BorrowExpr, start, GetNode(expr).end_token,
            { .borrow = { .operand = expr, .is_mut = is_mut } });
    }

    uint32_t Parser::ParseMutExpr()
    {
        uint32_t mut_tok = CurrToken(); NextToken(); // consume 'mut'
        if (CurrTokenIs(TokenType::AMPERSAND))
            return AddError("Write '&mut x' instead of 'mut &x'", mut_tok);
        if (CurrTokenIs(TokenType::NEW))
            return ParseNewExpr(mut_tok, 1);
        return AddError("Expected 'new' after 'mut'", mut_tok);
    }

    uint32_t Parser::ParseNewExpr(uint32_t start, uint8_t is_mut)
    {
        NextToken(); // consume 'new'
        uint32_t type = ParseNamedType();
        if (type == ERROR_NODE) return ERROR_NODE;

        uint32_t args;
        if (CurrTokenIs(TokenType::LPAREN))
        {
            StructLitScope allow(m_NoStructLit, false);
            args = ParseDelimitedList(
                NodeType::ArgList,
                TokenType::RPAREN,
                TokenType::COMMA,
                [this]() { return ParseExpr(PrecedenceType::P_LOWEST); },
                "Expected ',' or ')' in constructor argument list"
            );
        }
        else if (CurrTokenIs(TokenType::LBRACE))
        {
            if (m_NoStructLit)
                return AddError("Struct initializer is not allowed here; wrap the expression in parentheses");
            StructLitScope allow(m_NoStructLit, false);
            args = ParseDelimitedList(
                NodeType::FieldInitList,
                TokenType::RBRACE,
                TokenType::COMMA,
                [this]() { return ParseFieldInit(); },
                "Expected ',' or '}' in field initializer list"
            );
        }
        else return AddError("Expected '(' or '{' after type in new expression", type);

        if (args == ERROR_NODE) return ERROR_NODE;
        return PushNode(NodeType::NewExpr, start, GetNode(args).end_token,
            { .new_expr = { .type = type, .args = args, .is_mut = is_mut } });
    }

    uint32_t Parser::ParseParenExpr()
    {
        StructLitScope allow(m_NoStructLit, false);

        // '()' is the unit tuple
        if (PeekTokenIs(TokenType::RPAREN))
        {
            return ParseDelimitedList(
                NodeType::Tuple, TokenType::RPAREN, TokenType::COMMA,
                [this]() { return ParseExpr(PrecedenceType::P_LOWEST); },
                "Expected ',' or ')' in tuple expression"
            );
        }

        uint32_t open = CurrToken(); NextToken(); // consume '('
        uint32_t first = ParseExpr(PrecedenceType::P_LOWEST);
        if (first == ERROR_NODE) return ERROR_NODE;

        // '(a)' is grouping, '(a,)' and '(a, b)' are tuples
        if (CurrTokenIs(TokenType::COMMA))
        {
            size_t marker = GetScratchMarker();
            PushScratch(first);
            while (CurrTokenIs(TokenType::COMMA))
            {
                NextToken(); // consume ','
                if (CurrTokenIs(TokenType::RPAREN)) break; // trailing comma
                uint32_t elem = ParseExpr(PrecedenceType::P_LOWEST);
                if (elem == ERROR_NODE) { RollbackScratch(marker); return ERROR_NODE; }
                PushScratch(elem);
            }
            if (!CurrTokenIs(TokenType::RPAREN))
            {
                RollbackScratch(marker);
                return AddError("Expected ',' or ')' in tuple expression");
            }
            uint32_t close = CurrToken(); NextToken(); // consume ')'
            auto list = CommitList(marker);
            return PushNode(NodeType::Tuple, open, close, { .list = { .start = list.start, .count = list.count } });
        }

        if (!CurrTokenIs(TokenType::RPAREN)) return AddError("Expected ')' after expression", first);
        uint32_t close = CurrToken(); NextToken(); // consume ')'
        return PushNode(NodeType::ParenExpr, open, close, { .child = first });
    }

    uint32_t Parser::ParseStructInit()
    {
        uint32_t name = ParseIdentifier();
        StructLitScope allow(m_NoStructLit, false);

        uint32_t fields = ParseDelimitedList(
            NodeType::FieldInitList,
            TokenType::RBRACE,
            TokenType::COMMA,
            [this]() { return ParseFieldInit(); },
            "Expected ',' or '}' in field initializer list"
        );
        if (fields == ERROR_NODE) return ERROR_NODE;

        return PushNode(NodeType::StructInit, GetNode(name).start_token, GetNode(fields).end_token,
            { .pair = { .first = name, .second = fields } });
    }

    uint32_t Parser::ParseFieldInit()
    {
        if (!CurrTokenIs(TokenType::IDENT)) return AddError("Expected field name in initializer");
        uint32_t name = ParseIdentifier();
        uint32_t value = name; // shorthand 'x' means 'x: x'
        if (CurrTokenIs(TokenType::COLON))
        {
            NextToken();
            value = ParseExpr(PrecedenceType::P_LOWEST);
            if (value == ERROR_NODE) return ERROR_NODE;
        }

        return PushNode(NodeType::FieldInit, GetNode(name).start_token, GetNode(value).end_token,
            { .pair = { .first = name, .second = value } });
    }

    uint32_t Parser::ParseArrowBody()
    {
        NextToken(); // consume '=>'
        if (CurrTokenIs(TokenType::LBRACE) || CurrTokenIs(TokenType::LABEL))
            return AddError("'=>' takes a single expression; use '{ }' or '=>', not both");
        return ParseExpr(PrecedenceType::P_LOWEST);
    }

    uint32_t Parser::ParseBlockExpr()
    {
        DepthGuard guard(m_Depth);
        if (m_Depth > MAX_DEPTH) return AddError("Nesting is too deep");
        StructLitScope allow(m_NoStructLit, false);
        uint32_t start = CurrToken(), end; NextToken(); // consume '{'
        size_t marker = GetScratchMarker();

        while (!CurrTokenIs(TokenType::RBRACE) && !CurrTokenIs(TokenType::END_OF_FILE))
        {
            uint32_t stmt = ParseStatement();
            if (stmt == ERROR_NODE) Synchronize();
            else PushScratch(stmt);
        }

        if (!CurrTokenIs(TokenType::RBRACE))
        {
            RollbackScratch(marker);
            return AddError("Expected '}' at end of block expression");
        }
        end = CurrToken(); NextToken(); // consume '}'

        auto list = CommitList(marker);
        return PushNode(NodeType::BlockExpr, start, end,
            { .list = { .start = list.start, .count = list.count } });
    }

    uint32_t Parser::ParseIfExpr()
    {
        DepthGuard guard(m_Depth);
        if (m_Depth > MAX_DEPTH) return AddError("Nesting is too deep");
        uint32_t start = CurrToken(), end, cond; NextToken(); // consume 'if'
        {
            StructLitScope restrict_literals(m_NoStructLit, true);
            cond = ParseExpr(PrecedenceType::P_LOWEST);
        }
        if (cond == ERROR_NODE) return ERROR_NODE;

        uint32_t then_branch = ERROR_NODE;
        if (CurrTokenIs(TokenType::FATARROW)) then_branch = ParseArrowBody();
        else if (CurrTokenIs(TokenType::LBRACE)) then_branch = ParseBlockExpr();
        else return AddError("Expected '=>' or '{' after if condition", cond);

        if (then_branch == ERROR_NODE) return ERROR_NODE;

        uint32_t else_branch = ERROR_NODE;
        if (CurrTokenIs(TokenType::ELSE))
        {
            NextToken(); // consume 'else'
            if (CurrTokenIs(TokenType::IF)) else_branch = ParseIfExpr();
            else if (CurrTokenIs(TokenType::LBRACE)) else_branch = ParseBlockExpr();
            else if (CurrTokenIs(TokenType::FATARROW)) else_branch = ParseArrowBody();
            else return AddError("Expected 'if', '=>', or '{' after 'else'");

            if (else_branch == ERROR_NODE) return ERROR_NODE;
        }

        end = (else_branch != ERROR_NODE) ? GetNode(else_branch).end_token : GetNode(then_branch).end_token;
        return PushNode(NodeType::IfExpr, start, end,
            { .if_expr = { .cond = cond, .then_branch = then_branch, .else_branch = else_branch } });
    }

    uint32_t Parser::ParseForExpr()
    {
        uint32_t start = CurrToken(), end, cond; NextToken(); // consume 'for'

        // form: for {}
        if (CurrTokenIs(TokenType::LBRACE)) // parse body
        {
            uint32_t body = ParseBlockExpr();
            if (body == ERROR_NODE) return ERROR_NODE;
            end = GetNode(body).end_token;
            return PushNode(NodeType::ForExpr, start, end,
                { .for_expr = { .cond = ERROR_NODE, .body = body } });
        }

        // form: for item in items {}
        if (CurrTokenIs(TokenType::IDENT) && PeekTokenIs(TokenType::IN))
        {
            uint32_t ident = ParseIdentifier(), iterable;
            NextToken(); // consume 'in'

            { //parse iterable
                StructLitScope restrict_literals(m_NoStructLit, true);
                iterable = ParseExpr(PrecedenceType::P_LOWEST);
            }
            if (iterable == ERROR_NODE) return ERROR_NODE;

            if (!CurrTokenIs(TokenType::LBRACE))
                return AddError("Expected '{' to begin body block of 'for' expression", iterable);

            uint32_t body = ParseBlockExpr();
            if (body == ERROR_NODE) return ERROR_NODE;
            end = GetNode(body).end_token;

            return PushNode(NodeType::ForInExpr, start, end,
                { .for_in = { .var = ident, .iterable = iterable, .body = body } });
        }

        // form: for cond {}
        { // parse condition
            StructLitScope restrict_literals(m_NoStructLit, true);
            cond = ParseExpr(PrecedenceType::P_LOWEST);
        }
        if (cond == ERROR_NODE) return ERROR_NODE;
        if (!CurrTokenIs(TokenType::LBRACE))
            return AddError("Expected '{' to begin body block of 'for' expression", cond);
        uint32_t body = ParseBlockExpr(); // parse body
        if (body == ERROR_NODE) return ERROR_NODE;
        end = GetNode(body).end_token;
        return PushNode(NodeType::ForExpr, start, end,
            { .for_expr = { .cond = cond, .body = body } });
    }

    // statements
    uint32_t Parser::ParseStatement()
    {
        while (CurrTokenIs(TokenType::SEMICOLON)) NextToken(); // consume all leftover ';'
        if (CurrTokenIs(TokenType::END_OF_FILE) || CurrTokenIs(TokenType::RBRACE)) return ERROR_NODE;

        switch (GetCurrToken().type)
        {
            case TokenType::LET: return ParseLetStmt();
            case TokenType::RETURN: return ParseReturnStmt();
            case TokenType::BREAK: return ParseBreakStmt();
            case TokenType::CONTINUE: return ParseContinueStmt();
            case TokenType::FUNC: return ParseFuncDecl(CurrToken(), 0);
            case TokenType::STRUCT: return ParseStructDecl(CurrToken(), 0);
            case TokenType::PUB:
            {
                uint32_t start = CurrToken();
                NextToken();
                if (CurrTokenIs(TokenType::FUNC))   return ParseFuncDecl(start, 1);
                if (CurrTokenIs(TokenType::STRUCT)) return ParseStructDecl(start, 1);
                return AddError("Expected 'func' or 'struct' after 'pub'", start);
            }
            default: return DefaultParser();
        }
    }

    uint32_t Parser::DefaultParser()
    {
        bool block_like = IsBlockLikeStart(GetCurrToken().type);
        uint32_t expr = block_like ? ParseBlockLikeExpr() : ParseExpr(PrecedenceType::P_LOWEST);
        if (expr == ERROR_NODE) return ERROR_NODE;
        if (!block_like && CurrTokenIs(TokenType::EQ)) return ParseAssignStmt(expr);

        if (!block_like && m_Nodes[expr].type != NodeType::CallExpr)
            return AddError("Expression is not valid as a statement; only calls can be used as statements", expr, ErrorScope::NodeStart);

        uint32_t start = GetNode(expr).start_token, end = GetNode(expr).end_token;
        bool closed_by_brace = block_like && m_Tokens[end].type == TokenType::RBRACE;

        if (CurrTokenIs(TokenType::SEMICOLON)) { end = CurrToken(); NextToken(); }
        else if (!closed_by_brace)
            return AddError("Expected ';' after expression statement", expr, ErrorScope::TokenBefore);

        return PushNode(NodeType::ExprStmt, start, end, { .child = expr });
    }

    uint32_t Parser::ParseAssignStmt(uint32_t lhs)
    {
        NodeType lt = m_Nodes[lhs].type;
        if (lt != NodeType::Identifier &&
            lt != NodeType::FieldAccess &&
            lt != NodeType::IndexExpr   &&
            lt != NodeType::DerefExpr)
            return AddError("Invalid assignment target", lhs, ErrorScope::NodeStart);

        NextToken();
        uint32_t rhs = ParseExpr(PrecedenceType::P_LOWEST);
        if (rhs == ERROR_NODE) return ERROR_NODE;
        if (!CurrTokenIs(TokenType::SEMICOLON))
            return AddError("Expected ';' after assignment statement", rhs, ErrorScope::TokenBefore);

        uint32_t start = GetNode(lhs).start_token, end = CurrToken(); NextToken();
        return PushNode(NodeType::AssignStmt, start, end,
            { .binary = { .lhs = lhs, .op = Operator::Assign, .rhs = rhs } });
    }

    uint32_t Parser::ParseLetStmt()
    {
        uint32_t start = CurrToken(), end; NextToken();
        uint8_t is_mut = 0;
        if (CurrTokenIs(TokenType::MUT)) { is_mut = 1; NextToken(); }

        if (!CurrTokenIs(TokenType::IDENT)) return AddError("Expected identifier after 'let'");
        uint32_t ident = ParseIdentifier();

        uint32_t type_annot = ERROR_NODE;
        if (CurrTokenIs(TokenType::COLON))
        {
            NextToken();
            type_annot = ParseTypeAnnotation();
            if (type_annot == ERROR_NODE) return ERROR_NODE;
        }

        uint32_t target;
        if (type_annot == ERROR_NODE && is_mut == 0) target = ident;
        else
        {
            uint32_t t_start = GetNode(ident).start_token;
            uint32_t t_end = (type_annot != ERROR_NODE) ? GetNode(type_annot).end_token : GetNode(ident).end_token;
            target = PushNode(NodeType::TypedIdentifier, t_start, t_end,
                { .typed_ident = { .ident = ident, .type = type_annot, .is_mut = is_mut } });
        }

        if (!CurrTokenIs(TokenType::EQ)) 
            return AddError("Expected '=' in variable declaration", target);
        NextToken(); // consume '='

        uint32_t value = ParseExpr(PrecedenceType::P_LOWEST);
        if (value == ERROR_NODE) return ERROR_NODE;

        if (!CurrTokenIs(TokenType::SEMICOLON)) 
            return AddError("Expected ';' after let statement", value, ErrorScope::TokenBefore);
        end = CurrToken(); NextToken();
        return PushNode(NodeType::LetStmt, start, end,
            { .let = { .target = target, .value = value } });
    }
    
    uint32_t Parser::ParseReturnStmt()
    {
        uint32_t ret_tok = CurrToken(), end; NextToken();
        uint32_t expr = ERROR_NODE;

        if (!CurrTokenIs(TokenType::SEMICOLON))
        {
            expr = ParseExpr(PrecedenceType::P_LOWEST);
            if (expr == ERROR_NODE) return ERROR_NODE;
        }

        if (!CurrTokenIs(TokenType::SEMICOLON))
            return AddError("Expected ';' after return statement", expr, ErrorScope::TokenBefore);
        end = CurrToken(); NextToken();

        return PushNode(NodeType::ReturnStmt, ret_tok, end, { .child = expr });
    }

    uint32_t Parser::ParseBreakStmt()
    {   
        uint32_t break_tok = CurrToken(); NextToken(); // consume 'break'
        uint32_t label = ERROR_NODE, value = ERROR_NODE;

        if (CurrTokenIs(TokenType::LABEL))
        {
            label = PushNode(NodeType::Label, CurrToken());
            NextToken();
        }

        if (!CurrTokenIs(TokenType::SEMICOLON))
        {
            value = ParseExpr(PrecedenceType::P_LOWEST);
            if (value == ERROR_NODE) return ERROR_NODE;
        }

        if (!CurrTokenIs(TokenType::SEMICOLON)) 
        {
            uint32_t ref = (value != ERROR_NODE) ? value 
                        : (label != ERROR_NODE ? label : break_tok);
            return AddError("Expected ';' after break statement", ref, ErrorScope::TokenBefore);
        }

        uint32_t end = CurrToken(); NextToken(); // consume ';'

        return PushNode(NodeType::BreakStmt, break_tok, end,
            { .break_stmt = { .label = label, .value = value } });
    }


    uint32_t Parser::ParseContinueStmt()
    {
        uint32_t cont_tok = CurrToken(), end; NextToken();
        uint32_t label = ERROR_NODE;

        if (CurrTokenIs(TokenType::LABEL))
        {
            label = PushNode(NodeType::Label, CurrToken());
            NextToken();
        }

        if (!CurrTokenIs(TokenType::SEMICOLON)) 
        {
            uint32_t ref = (label != ERROR_NODE) ? label : cont_tok;
            return AddError("Expected ';' after continue statement", ref, ErrorScope::TokenBefore);
        }
        end = CurrToken(); NextToken();
        return PushNode(NodeType::ContinueStmt, cont_tok, end,
            { .continue_stmt = { .label = label } });
    }

    // declarations
    uint32_t Parser::ParseFuncDecl(uint32_t start, uint8_t is_pub)
    {
        NextToken(); // consume 'func'
        if (!CurrTokenIs(TokenType::IDENT)) return AddError("Expected function name after 'func'");
        uint32_t name = ParseIdentifier();

        if (!CurrTokenIs(TokenType::LPAREN)) return AddError("Expected '(' after function name", name);

        uint32_t params = ParseDelimitedList(
            NodeType::ArgList,
            TokenType::RPAREN,
            TokenType::COMMA,
            [this]() { return ParseTypedIdent("Expected parameter name"); },
            "Expected ',' or ')' in parameter list"
        );
        if (params == ERROR_NODE) return ERROR_NODE;
        uint32_t rparen = GetNode(params).end_token;

        uint32_t ret_type = ERROR_NODE;
        if (CurrTokenIs(TokenType::ARROW))
        {
            NextToken(); // consume '->'
            ret_type = ParseTypeAnnotation();
            if (ret_type == ERROR_NODE) return ERROR_NODE;
        }

        uint32_t sign_end = (ret_type != ERROR_NODE) ? GetNode(ret_type).end_token : rparen;
        uint32_t sign = PushNode(NodeType::FuncSign,
            GetNode(name).start_token, sign_end,
            { .func_sign = { .name = name, .params = params, .ret_type = ret_type } });

        if (!CurrTokenIs(TokenType::LBRACE))
            return AddError("Expected '{' for function body", sign);

        uint32_t body = ParseBlockExpr();
        if (body == ERROR_NODE) return ERROR_NODE;

        uint32_t end = GetNode(body).end_token;
        return PushNode(NodeType::FuncDecl, start, end,
            { .func_decl = { .sign = sign, .body = body, .is_pub = is_pub } });
    }

    uint32_t Parser::ParseStructDecl(uint32_t start, uint8_t is_pub)
    {
        NextToken(); // consume 'struct'
        if (!CurrTokenIs(TokenType::IDENT)) return AddError("Expected struct name after 'struct'");
        uint32_t name = ParseIdentifier();

        // unit form
        if (CurrTokenIs(TokenType::SEMICOLON))
        {
            uint32_t end = CurrToken(); NextToken();
            return PushNode(NodeType::StructDecl, start, end,
                { .struct_decl = { .name = name, .fields = ERROR_NODE, .is_pub = is_pub } });
        }

        if (!CurrTokenIs(TokenType::LBRACE))
            return AddError("Expected '{' or ';' after struct name", name);

        uint32_t fields = ParseDelimitedList(
            NodeType::ArgList,
            TokenType::RBRACE,
            TokenType::COMMA,
            [this]() { return ParseTypedIdent("Expected field name in struct body"); },
            "Expected ',' or '}' in struct body"
        );
        if (fields == ERROR_NODE) return ERROR_NODE;

        return PushNode(NodeType::StructDecl, start, GetNode(fields).end_token,
            { .struct_decl = { .name = name, .fields = fields, .is_pub = is_pub } });
    }

    uint32_t Parser::ParseTypedIdent(std::string_view missing_name_message)
    {
        if (!CurrTokenIs(TokenType::IDENT)) return AddError(missing_name_message);
        uint32_t ident = ParseIdentifier();

        if (!CurrTokenIs(TokenType::COLON)) return AddError("Expected ':' after name", ident);
        NextToken();

        uint32_t type = ParseTypeAnnotation();
        if (type == ERROR_NODE) return ERROR_NODE;

        return PushNode(NodeType::TypedIdentifier,
            GetNode(ident).start_token, GetNode(type).end_token,
            { .typed_ident = { .ident = ident, .type = type, .is_mut = 0 } });
    }

    // helpers
    uint32_t Parser::ParseIdentifier()
    {
        uint32_t node = PushNode(NodeType::Identifier, CurrToken());
        NextToken(); 
        return node;
    }

    uint32_t Parser::ParseTypeAnnotation()
    {
        DepthGuard guard(m_Depth);
        if (m_Depth > MAX_DEPTH) return AddError("Nesting is too deep");
        uint32_t start = CurrToken();

        switch (GetCurrToken().type)
        {
            case TokenType::MUT:
            {
                uint32_t mut_tok = CurrToken(); NextToken();
                if (CurrTokenIs(TokenType::AMPERSAND))
                    return AddError("Write '&mut T' instead of 'mut &T'", mut_tok);
                return AddError("Expected '&' after 'mut' in type; 'mut' only applies to references", mut_tok);
            }
            case TokenType::AMPERSAND:
            {
                NextToken(); // consume '&'
                uint8_t is_mut = 0;
                if (CurrTokenIs(TokenType::MUT))
                {
                    is_mut = 1;
                    NextToken(); // consume 'mut'
                }
                return ParseRefType(start, is_mut);
            }
            case TokenType::LPAREN: return ParseTupleType();
            case TokenType::DYN:    return ParseDynType(); 
            case TokenType::IDENT:  return ParseNamedType();
            default: return AddError("Expected type annotation");
        }
    }

    uint32_t Parser::ParseRefType(uint32_t start, uint8_t is_mut)
    {
        uint32_t inner = ParseTypeAnnotation();
        if (inner == ERROR_NODE) return ERROR_NODE;

        return PushNode(NodeType::RefType, start, GetNode(inner).end_token,
            { .ref_type = { .inner = inner, .is_mut = is_mut } });
    }

    uint32_t Parser::ParseNamedType()
    {
        if (!CurrTokenIs(TokenType::IDENT)) return AddError("Expected type identifier in type annotation");

        uint32_t ident_tok = CurrToken(), end = ident_tok;
        NextToken();

        uint32_t args = ERROR_NODE;
        if (CurrTokenIs(TokenType::LT))
        {
            args = ParseDelimitedList(
                NodeType::TypeArgList,
                TokenType::GT,
                TokenType::COMMA,
                [this]() { return ParseTypeAnnotation(); },
                "Expected ',' or '>' in type arguments"
            );
            if (args == ERROR_NODE) return ERROR_NODE;
            end = GetNode(args).end_token;
        }

        return PushNode(NodeType::NamedType, ident_tok, end,
            { .named_type = { .ident = ident_tok, .args = args } });
    }

    uint32_t Parser::ParseDynType()
    {
        uint32_t start = CurrToken();
        NextToken(); // consume 'dyn'

        uint32_t inner = ParseNamedType();
        if (inner == ERROR_NODE)
            return AddError("Expected trait/class name after 'dyn'");

        return PushNode(NodeType::DynType, start, GetNode(inner).end_token,
            { .child = inner });
    }

    uint32_t Parser::ParseTupleType()
    {
        return ParseDelimitedList(
            NodeType::TupleType,
            TokenType::RPAREN,
            TokenType::COMMA,
            [this]() { return ParseTypeAnnotation(); },
            "Expected ',' or ')' in tuple type"
        );
    }

    // other
    bool Parser::IsStructInitAhead() const
    {
        TokenType first = PeekType(2);
        if (first == TokenType::RBRACE) return true;
        if (first != TokenType::IDENT) return false;

        TokenType second = PeekType(3);
        return second == TokenType::COLON || second == TokenType::COMMA || second == TokenType::RBRACE;
    }

    void Parser::Synchronize()
    {
        uint32_t depth = 0;
        while (!CurrTokenIs(TokenType::END_OF_FILE))
        {
            TokenType t = GetCurrToken().type;
            if (t == TokenType::LBRACE) ++depth;
            else if (t == TokenType::RBRACE)
            {
                if (depth == 0) return; 
                --depth;
            }
            else if (depth == 0)
            {
                if (t == TokenType::SEMICOLON) { NextToken(); return; }
                if (t == TokenType::LET      || t == TokenType::RETURN || t == TokenType::BREAK ||
                    t == TokenType::CONTINUE || t == TokenType::FUNC   || t == TokenType::STRUCT ||
                    t == TokenType::PUB) return;
            }
            NextToken();
        }
    }

    uint32_t Parser::AddError(std::string_view message, uint32_t node, ErrorScope scope)
    {
        uint32_t pos = CurrToken();

        switch (scope)
        {
            case ErrorScope::NodeStart:
                if (node != ERROR_NODE && node < m_Nodes.size()) pos = m_Nodes[node].start_token;
                break;
            case ErrorScope::TokenBefore:
                if (m_Cursor > 0) pos = m_Cursor - 1;
                break;
            case ErrorScope::Token:
            default: break;
        }
        if (pos >= m_Tokens.size()) pos = static_cast<uint32_t>(m_Tokens.size()) - 1;

        m_Errors.push_back(ParseError
        {
            .message = std::string(message),
            .node    = node,
            .token   = pos,
            .scope   = scope
        });
        return ERROR_NODE;
    }

    ListHeader Parser::CommitList(size_t marker)
    {
        uint32_t count = static_cast<uint32_t>(m_Scratch.size() - marker);
        uint32_t start = static_cast<uint32_t>(m_Extra.size());
        m_Extra.insert(m_Extra.end(), m_Scratch.begin() + marker, m_Scratch.end());
        m_Scratch.resize(marker);
        return { .start = start, .count = count };
    }
}