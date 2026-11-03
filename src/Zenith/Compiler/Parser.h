#pragma once

#include <string>
#include <vector>
#include <string_view>
#include <iostream>
#include "Zenith/Compiler/Lexer.h"
#include "Zenith/Compiler/Ast.h"

namespace Zenith {

    constexpr uint32_t ERROR_NODE = 0;

    enum class ErrorScope : uint8_t
    {
        Token,       
        NodeStart,    
        TokenBefore, 
    };

    struct ParseError
    {
        std::string message;
        uint32_t node  = ERROR_NODE;
        uint32_t token = 0;
        ErrorScope scope = ErrorScope::Token;
    };

    struct ListHeader 
    {
        uint32_t start;
        uint32_t count;
    };

    struct StructLitScope
    {
        bool& flag;
        bool saved;
        StructLitScope(bool& f, bool restrictLiterals) : flag(f), saved(f) { flag = restrictLiterals; }
        ~StructLitScope() { flag = saved; }
    };

    enum class PrecedenceType : uint8_t 
    {
        P_LOWEST = 0,
        P_OR,
        P_AND,
        P_EQUALS,
        P_LESSGREATER,
        P_SUM,
        P_PRODUCT,
        P_UNARY,
        P_CALL,
        P_INDEX
    };

    class Parser 
    {
    public:
        Parser(std::string_view source);
        AstView ParseProgram();

        void DumpErrors(std::ostream& out = std::cerr) const;
        void DumpFlatAst(std::ostream& out = std::cout) const;
        void DumpTreeAst(std::ostream& out = std::cout, uint32_t rootIdx = ERROR_NODE) const;

        bool HasErrors() const { return !m_Errors.empty(); }
        uint32_t GetErrorsCount() const { return static_cast<uint32_t>(m_Errors.size()); }
    private:
        std::string_view m_Source;
        uint32_t m_Cursor = 0;
        std::vector<Node> m_Nodes;
        std::vector<Token> m_Tokens;
        std::vector<uint32_t> m_Extra;
        std::vector<uint32_t> m_Scratch;
        std::vector<ParseError> m_Errors;
        Lexer m_Lexer;
        bool m_NoStructLit = false;

        // recursion guard: chống stack overflow với input lồng quá sâu
        static constexpr uint32_t MAX_DEPTH = 256;
        uint32_t m_Depth = 0;
        struct DepthGuard
        {
            uint32_t& depth;
            explicit DepthGuard(uint32_t& d) : depth(d) { ++depth; }
            ~DepthGuard() { --depth; }
        };

        // error handler
        void Synchronize();
        uint32_t AddError(std::string_view message, uint32_t node = ERROR_NODE, ErrorScope scope = ErrorScope::Token);

        // dump helpers
        void DumpTreeNode(std::ostream& out, uint32_t nodeIdx, int depth) const;
        void DumpTreeList(std::ostream& out, uint32_t start, uint32_t count, int depth) const;
        std::string NodeTypeToString(NodeType type) const;

        // buffer's methods
        size_t GetScratchMarker() const { return m_Scratch.size(); }
        void PushScratch(uint32_t node) { m_Scratch.push_back(node); }
        void RollbackScratch(size_t marker) { m_Scratch.resize(marker); }
        ListHeader CommitList(size_t marker);

        uint32_t NextToken() { return ++m_Cursor; }
        uint32_t CurrToken() { return m_Cursor; }
        const Token& GetToken(uint32_t idx) const { return m_Tokens[idx]; }

        const Token& GetCurrToken() const { return GetToken(m_Cursor); }
        const Token& GetPeekToken() const { return GetToken(m_Cursor + 1); }
        TokenType PeekType(uint32_t n) const
        {
            size_t idx = m_Cursor + n;
            if (idx >= m_Tokens.size()) idx = m_Tokens.size() - 1;
            return m_Tokens[idx].type;
        }
        bool CurrTokenIs(TokenType type) { return GetToken(m_Cursor).type == type; }
        bool PeekTokenIs(TokenType type) { return GetToken(m_Cursor + 1).type == type; }

        const Node& GetNode(uint32_t idx) const { return m_Nodes[idx]; }
        uint32_t GetCurrNode() { return static_cast<uint32_t>(m_Nodes.size() - 1); }
        uint32_t GetListStart() { return static_cast<uint32_t>(m_Extra.size()); }
        void PushList(uint32_t elem, uint32_t& count) { m_Extra.push_back(elem); ++count; }

        uint32_t PushNode(NodeType type, uint32_t start, uint32_t end, Node::Data data = {}) 
        {
            m_Nodes.push_back(Node{ .type = type, .start_token = start, .end_token = end, .data = data });
            return static_cast<uint32_t>(m_Nodes.size() - 1);
        }

        uint32_t PushNode(NodeType type, uint32_t start, Node::Data data = {}) 
        {
            m_Nodes.push_back(Node{ .type = type, .start_token = start, .end_token = start, .data = data });
            return static_cast<uint32_t>(m_Nodes.size() - 1);
        }

        // parsing methods
        uint32_t ParseStatement();
        uint32_t DefaultParser();
        uint32_t ParseLetStmt();
        uint32_t ParseReturnStmt();
        uint32_t ParseBreakStmt();
        uint32_t ParseContinueStmt();
        uint32_t ParseAssignStmt(uint32_t lhs);
        uint32_t ParseFuncDecl(uint32_t start, uint8_t isPub = 0);
        uint32_t ParseTypedIdent(std::string_view missingNameMessage);
        uint32_t ParseStructDecl(uint32_t start, uint8_t isPub = 0);

        uint32_t ParsePrefixExpr();
        uint32_t ParseInfixExpr(uint32_t left);
        uint32_t ParseBlockLikeExpr();
        uint32_t ParseLabeledExpr();
        uint32_t ParseExpr(PrecedenceType precedence);

        uint32_t ParseNumericExpr(uint32_t left);
        uint32_t ParseFieldAccessExpr(uint32_t left);
        uint32_t ParseCallExpr(uint32_t left);
        uint32_t ParseIndexExpr(uint32_t left);
        uint32_t ParseBorrowExpr(uint32_t start);
        uint32_t ParseMutExpr();
        uint32_t ParseNewExpr(uint32_t start, uint8_t isMut);
        uint32_t ParseParenExpr();
        uint32_t ParseStructInit();
        uint32_t ParseFieldInit();
        uint32_t ParseArrowBody();
        uint32_t ParseBlockExpr();
        uint32_t ParseIfExpr();
        uint32_t ParseForExpr();

        uint32_t ParseIdentifier();
        uint32_t ParseTypeAnnotation();
        uint32_t ParseRefType(uint32_t start, uint8_t isMut);
        uint32_t ParseNamedType();
        uint32_t ParseDynType();
        uint32_t ParseTupleType();

        bool IsStructInitAhead() const;

        //utils
        constexpr PrecedenceType GetPrecedence(TokenType type) noexcept 
        {
            switch (type) 
            {
                case TokenType::OR:         return PrecedenceType::P_OR;
                case TokenType::AND:        return PrecedenceType::P_AND;
                case TokenType::EE:         
                case TokenType::NE:         return PrecedenceType::P_EQUALS;
                case TokenType::LT:
                case TokenType::GT:
                case TokenType::LTE:
                case TokenType::GTE:        return PrecedenceType::P_LESSGREATER;
                case TokenType::PLUS:
                case TokenType::MINUS:      return PrecedenceType::P_SUM;
                case TokenType::ASTERISK:
                case TokenType::SLASH:
                case TokenType::MODULUS:    return PrecedenceType::P_PRODUCT;
                case TokenType::DOT:
                case TokenType::LPAREN:     return PrecedenceType::P_CALL;
                case TokenType::LBRACKET:   return PrecedenceType::P_INDEX;
                default:                    return PrecedenceType::P_LOWEST;
            }
        }

        constexpr bool IsBlockLikeStart(TokenType type) noexcept
        {
            switch (type)
            {
                case TokenType::LBRACE:
                case TokenType::IF:
                case TokenType::FOR:
                case TokenType::LABEL:  return true;
                default:                return false;
            }
        }

        template <typename Fn>
        uint32_t ParseDelimitedList(
            NodeType resType, 
            TokenType closeType, 
            TokenType sepType, 
            Fn parseElement, 
            std::string_view errorMessage,
            bool allowTrailing = true)
        {
            uint32_t start = CurrToken(); NextToken(); // consume open_token
            size_t marker = GetScratchMarker();

            if (!CurrTokenIs(closeType))  
            {
                while (true)
                {
                    uint32_t elem = parseElement();
                    if (elem == ERROR_NODE)
                    {
                        RollbackScratch(marker);
                        return ERROR_NODE;
                    }
                    PushScratch(elem);

                    if (CurrTokenIs(closeType)) break; 
                    else if (CurrTokenIs(sepType))
                    {
                        NextToken(); // consume separator
                        if (allowTrailing && CurrTokenIs(closeType)) break; // trailing: (1, 2, )
                    }
                    else
                    {
                        RollbackScratch(marker);
                        return AddError(errorMessage);
                    }
                }
            }

            uint32_t end = CurrToken(); NextToken(); // consume close_type
            auto list = CommitList(marker);
            return PushNode(resType, start, end, { .list = { .start = list.start, .count = list.count } });
        }
    };
}
