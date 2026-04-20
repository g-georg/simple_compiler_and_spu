#pragma once

#include <memory>
#include <stdexcept>
#include "Token.hpp"

struct AstNode
{
    NodeType  type  = NodeType::Unknown;
    NodeValue value = {};

    std::unique_ptr<AstNode> left;
    std::unique_ptr<AstNode> right;

    static std::unique_ptr<AstNode> makeNum(ValueNumber n)
    {
        auto node    = std::make_unique<AstNode>();
        node->type   = NodeType::ConstNum;
        node->value.number = n;
        return node;
    }

    static std::unique_ptr<AstNode> makeName(size_t idx)
    {
        auto node    = std::make_unique<AstNode>();
        node->type   = NodeType::Name;
        node->value.idx = idx;
        return node;
    }

    static std::unique_ptr<AstNode> makeKeyword(
            Keyword kw,
            std::unique_ptr<AstNode> l = nullptr,
            std::unique_ptr<AstNode> r = nullptr)
    {
        auto node       = std::make_unique<AstNode>();
        node->type      = NodeType::Keyword;
        node->value.idx = static_cast<size_t>(kw);
        node->left      = std::move(l);
        node->right     = std::move(r);
        return node;
    }

    std::unique_ptr<AstNode> clone() const
    {
        auto c   = std::make_unique<AstNode>();
        c->type  = type;
        c->value = value;
        if (left)  c->left  = left->clone();
        if (right) c->right = right->clone();
        return c;
    }

    Keyword keyword() const { return static_cast<Keyword>(value.idx); }
};

using NodePtr = std::unique_ptr<AstNode>;

inline NodePtr NUM (ValueNumber n)              { return AstNode::makeNum(n);  }
inline NodePtr NAME(size_t idx)                 { return AstNode::makeName(idx); }

inline NodePtr KW(Keyword kw, NodePtr l = nullptr, NodePtr r = nullptr)
{
    return AstNode::makeKeyword(kw, std::move(l), std::move(r));
}

inline NodePtr CONNECT_(NodePtr l, NodePtr r) { return KW(Keyword::Connect,   std::move(l), std::move(r)); }
inline NodePtr ADD_    (NodePtr l, NodePtr r) { return KW(Keyword::Add,       std::move(l), std::move(r)); }
inline NodePtr SUB_    (NodePtr l, NodePtr r) { return KW(Keyword::Sub,       std::move(l), std::move(r)); }
inline NodePtr MUL_    (NodePtr l, NodePtr r) { return KW(Keyword::Mul,       std::move(l), std::move(r)); }
inline NodePtr DIV_    (NodePtr l, NodePtr r) { return KW(Keyword::Div,       std::move(l), std::move(r)); }
inline NodePtr POW_    (NodePtr l, NodePtr r) { return KW(Keyword::Pow,       std::move(l), std::move(r)); }
inline NodePtr IF_     (NodePtr l, NodePtr r) { return KW(Keyword::If,        std::move(l), std::move(r)); }
inline NodePtr ASSIGN_ (NodePtr l, NodePtr r) { return KW(Keyword::Assign,    std::move(l), std::move(r)); }
inline NodePtr DECLARE_(NodePtr l, NodePtr r) { return KW(Keyword::Declarate, std::move(l), std::move(r)); }
inline NodePtr PRINT_  (NodePtr l, NodePtr r) { return KW(Keyword::Print,     std::move(l), std::move(r)); }
inline NodePtr RETURN_ (NodePtr l, NodePtr r) { return KW(Keyword::Return,    std::move(l), std::move(r)); }
inline NodePtr CALL_   (NodePtr l, NodePtr r) { return KW(Keyword::Call,      std::move(l), std::move(r)); }
inline NodePtr FUNC_   (NodePtr l, NodePtr r) { return KW(Keyword::Func,      std::move(l), std::move(r)); }
inline NodePtr MAIN_   (NodePtr l, NodePtr r) { return KW(Keyword::Main,      std::move(l), std::move(r)); }
inline NodePtr COMMA_  (NodePtr l, NodePtr r) { return KW(Keyword::Comma,     std::move(l), std::move(r)); }
