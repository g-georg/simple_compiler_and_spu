#pragma once

#include <vector>
#include <unordered_set>
#include <string>
#include <stdexcept>

#include "AstNode.hpp"
#include "Tokenizer.hpp"

class Parser
{
public:
    NodePtr parse(std::vector<Token> tokens, NamesTable& names)
    {
        tokens_ = std::move(tokens);
        names_  = &names;
        cur_    = 0;

        return parseGramma();
    }

private:
    std::vector<Token> tokens_;
    NamesTable* names_ = nullptr;
    size_t cur_ = 0;

    std::unordered_set<size_t> declaredVars_;
    std::unordered_set<size_t> declaredFuncs_;

    const Token& peek() const
    {
        static const Token eof{};
        return cur_ < tokens_.size() ? tokens_[cur_] : eof;
    }

    bool atEnd() const { return cur_ >= tokens_.size(); }

    bool isKeyword(Keyword kw) const
    {
        return !atEnd() &&
               peek().type == NodeType::Keyword &&
               static_cast<Keyword>(peek().value.idx) == kw;
    }

    bool isNextKeyword(Keyword kw)   const
    {
        return cur_ + 1 < tokens_.size() &&
               tokens_[cur_ + 1].type == NodeType::Keyword &&
               static_cast<Keyword>(tokens_[cur_ + 1].value.idx) == kw;
    }

    bool isType(NodeType t) const { return !atEnd() && peek().type == t; }

    void advance() { if (!atEnd()) ++cur_; }

    void expect(Keyword kw, const char* msg)
    {
        if (!isKeyword(kw)) syntaxError(msg);
        advance();
    }

    [[noreturn]] void syntaxError(const std::string& msg) const
    {
        std::string full = "Syntax error at line " +
                           std::to_string(peek().line) + ":" +
                           std::to_string(peek().col) + " — " + msg;
        throw SyntaxError(full);
    }

    NodePtr parseGramma()
    {
        NodePtr root = tryParseTopLevel();
        if (!root)
            syntaxError("Program must start with a function or main (func / main)");

        root = CONNECT_(std::move(root), nullptr);

        while (!atEnd())
        {
            NodePtr next = tryParseTopLevel();
            if (!next)
                syntaxError("Expected function or main declaration");

            root = CONNECT_(std::move(root), std::move(next));
        }

        return root;
    }

    NodePtr tryParseTopLevel()
    {
        if (isKeyword(Keyword::Func)) return parseFunction();
        if (isKeyword(Keyword::Main)) return parseMain();
        return nullptr;
    }

    NodePtr parseFunction()
    {
        expect(Keyword::Func, "Expected 'func'");

        NodePtr node = FUNC_(nullptr, nullptr);
        node->left   = COMMA_(nullptr, nullptr);
        node->left->left = parseFuncName();

        parseFuncCommon(node);
        return node;
    }

    NodePtr parseMain()
    {
        expect(Keyword::Main, "Expected 'main'");

        NodePtr node = MAIN_(nullptr, nullptr);
        node->left = parseFuncName();

        parseFuncCommon(node);
        return node;
    }

    void parseFuncCommon(NodePtr& funcNode)
    {
        expect(Keyword::OpenParens,   "Expected '(' after function name");
        expect(Keyword::CloseParens,  "Expected ')' after '('");
        expect(Keyword::OpenBracket,  "Expected '{' ");

        while (!isKeyword(Keyword::Return))
        {
            NodePtr op = parseOperation();
            funcNode->right = CONNECT_(std::move(funcNode->right), std::move(op));
        }

        funcNode->right = CONNECT_(std::move(funcNode->right), nullptr);
        NodePtr& body   = funcNode->right;
        body->right     = parseReturn();

        expect(Keyword::CloseBracket, "Expected '}' ");
    }

    NodePtr parseFuncName()
    {
        if (!isType(NodeType::Name))
            syntaxError("Expected function name");

        size_t idx = peek().value.idx;

        if (declaredFuncs_.count(idx))
            syntaxError("Redeclaration of function '" +
                        names_->findByIdx(idx)->name + "'");

        declaredFuncs_.insert(idx);
        declaredVars_.insert(idx);

        advance();
        return NAME(idx);
    }

    NodePtr parseOperation()
    {
        if (isKeyword(Keyword::If))          return parseIf();
        if (isKeyword(Keyword::OpenBracket)) return parseBlock();

        if (auto n = tryDeclarateOrAssign()) return n;

        if (auto n = tryFunctionCall())
        {
            if (isKeyword(Keyword::Connect))
                advance();
            return n;
        }

        if (auto n = tryPrint()) return n;

        syntaxError("Unknown operation");
    }

    NodePtr parseIf()
    {
        expect(Keyword::If, "Expected 'if'");
        expect(Keyword::OpenParens, "Expected '(' after 'if'");

        NodePtr cond = parseExpression();

        expect(Keyword::CloseParens, "Expected ')' after if-condition");

        NodePtr body = parseOperation();

        return IF_(std::move(cond), std::move(body));
    }

    NodePtr parseBlock()
    {
        advance();

        NodePtr first = parseOperation();

        while (!isKeyword(Keyword::CloseBracket))
        {
            NodePtr next = parseOperation();
            first = CONNECT_(std::move(first), std::move(next));
        }

        advance();
        return first;
    }

   
    NodePtr tryDeclarateOrAssign()
    {
        if (!isType(NodeType::Name)) return nullptr;

        bool isDecl   = isNextKeyword(Keyword::Declarate);
        bool isAssign = isNextKeyword(Keyword::Assign);

        if (!isDecl && !isAssign) return nullptr;

        size_t idx = peek().value.idx;
        advance(); 

        Keyword op = static_cast<Keyword>(peek().value.idx);
        advance();

        NodePtr nameNode;
        if (isDecl)
        {
            if (declaredVars_.count(idx))
                syntaxError("Redeclaration of variable '" +
                            names_->findByIdx(idx)->name + "'");
            declaredVars_.insert(idx);
            nameNode = NAME(idx);
        }
        else
        {
            if (!declaredVars_.count(idx))
                syntaxError("Variable '" +
                            names_->findByIdx(idx)->name + "' used before declaration");
            nameNode = NAME(idx);
        }

        NodePtr expr = parseExpression();
        expect(Keyword::Connect, "Expected ';' after expression");
        return KW(op, std::move(nameNode), std::move(expr));
    }
    NodePtr tryPrint()
    {
        if (!isKeyword(Keyword::Print)) return nullptr;
        advance();

        expect(Keyword::OpenParens,  "Expected '(' after 'print'");
        NodePtr expr = parseExpression();
        expect(Keyword::CloseParens, "Expected ')' after print expression");
        expect(Keyword::Connect,     "Expected ';' after print statement");

        return CONNECT_(PRINT_(std::move(expr), nullptr), nullptr);
    }

    NodePtr parseReturn()
    {
        expect(Keyword::Return, "Expected return keyword");
        NodePtr expr = parseExpression();
        if (isKeyword(Keyword::Connect))
            advance();
        return CONNECT_(RETURN_(std::move(expr), nullptr), nullptr);
    }

    NodePtr tryReturn()
    {
        if (!isKeyword(Keyword::Return)) return nullptr;
        return parseReturn();
    }

    NodePtr parseExpression()
    {
        NodePtr node = parseTerm();

        while (isKeyword(Keyword::Add) || isKeyword(Keyword::Sub))
        {
            Keyword op = static_cast<Keyword>(peek().value.idx);
            advance();

            NodePtr rhs = parseTerm();

            if (op == Keyword::Add) node = ADD_(std::move(node), std::move(rhs));
            else                    node = SUB_(std::move(node), std::move(rhs));
        }

        return node;
    }

    NodePtr parseTerm()
    {
        NodePtr node = parsePower();

        while (isKeyword(Keyword::Mul) || isKeyword(Keyword::Div))
        {
            Keyword op = static_cast<Keyword>(peek().value.idx);
            advance();

            NodePtr rhs = parsePower();

            if (op == Keyword::Mul) node = MUL_(std::move(node), std::move(rhs));
            else                    node = DIV_(std::move(node), std::move(rhs));
        }

        return node;
    }

    NodePtr parsePower()
    {
        NodePtr node = parsePrimary();

        while (isKeyword(Keyword::Pow))
        {
            advance();
            NodePtr rhs = parsePrimary();
            node = POW_(std::move(node), std::move(rhs));
        }

        return node;
    }

    NodePtr parsePrimary()
    {
        if (isKeyword(Keyword::OpenParens))
        {
            advance();
            NodePtr expr = parseExpression();
            expect(Keyword::CloseParens, "Expected ')'");
            return expr;
        }

        if (isType(NodeType::ConstNum))
        {
            ValueNumber n = peek().value.number;
            advance();
            return NUM(n);
        }

        if (auto node = tryBuiltinFunction()) return node;

        if (auto node = tryFunctionCall())    return node;

        if (auto node = tryVariable())        return node;

        syntaxError("Expected primary expression");
    }

    NodePtr tryBuiltinFunction()
    {
        if (!isType(NodeType::Keyword)) return nullptr;

        Keyword kw = static_cast<Keyword>(peek().value.idx);
        const KeywordInfo* info = FindBuiltinFunction(kw);
        if (!info) return nullptr;

        advance();
        expect(Keyword::OpenParens, "Expected '(' after builtin function");

        NodePtr node = KW(kw);

        if (info->numberOfArgs == 0)
        {
            expect(Keyword::CloseParens, "Expected ')' after builtin function");
            return node;
        }

        NodePtr first = parseExpression();

        if (info->numberOfArgs == 1)
        {
            expect(Keyword::CloseParens, "Expected ')' after function argument");
            node->right = std::move(first);
            return node;
        }

        expect(Keyword::Comma, "Expected ',' between function arguments");
        NodePtr second = parseExpression();
        expect(Keyword::CloseParens, "Expected ')' after function arguments");

        node->left  = std::move(first);
        node->right = std::move(second);
        return node;
    }

    NodePtr tryFunctionCall()
    {
        if (!isKeyword(Keyword::Call)) return nullptr;
        advance();

        if (!isType(NodeType::Name))
            syntaxError("Expected function name after 'call'");

        size_t idx = peek().value.idx;
        if (!declaredFuncs_.count(idx) && !declaredVars_.count(idx))
            syntaxError("Call to undeclared function '" +
                        names_->findByIdx(idx)->name + "'");

        advance();

        expect(Keyword::OpenParens,  "Expected '(' after function name in call");
        expect(Keyword::CloseParens, "Expected ')' to close function call");

        return CALL_(NAME(idx), nullptr);
    }

    NodePtr tryVariable()
    {
        if (!isType(NodeType::Name)) return nullptr;

        size_t idx = peek().value.idx;

        if (!declaredVars_.count(idx))
            syntaxError("Variable '" +
                        names_->findByIdx(idx)->name + "' used before declaration");

        advance();
        return NAME(idx);
    }
};
