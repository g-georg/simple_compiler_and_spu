#pragma once

#include <cstddef>
#include <cstring>

enum class NodeType
{
    Unknown,
    ConstNum,
    Keyword,
    Name,
};

enum class Keyword
{
    Unknown,
    Add, Sub, Mul, Div, Pow,
    Log, Ln,
    Sin, Cos, Tg, Ctg,
    Arcsin, Arccos, Arctg, Arcctg,
    Sh, Ch, Th, Cth,
    Input, Print,
    Connect, Comma,
    Declarate, Assign,
    If,
    OpenParens, CloseParens,
    OpenBracket, CloseBracket,
    Func, Main, Return, Call,
};

using ValueNumber = int;

struct NodeValue
{
    ValueNumber number = 0;   
    size_t      idx    = 0;
};

struct KeywordInfo
{
    const char* langName;
    const char* standardName;
    Keyword     kw;
    bool        isFunction;
    size_t      numberOfArgs;
};

static const KeywordInfo kKeywords[] =
{
    {"input",   "input",   Keyword::Input,       true,  0},
    {"print",   "print",   Keyword::Print,        true,  1},
    {"arcsin",  "arcsin",  Keyword::Arcsin,       true,  1},
    {"arccos",  "arccos",  Keyword::Arccos,       true,  1},
    {"arctg",   "arctg",   Keyword::Arctg,        true,  1},
    {"arcctg",  "arcctg",  Keyword::Arcctg,       true,  1},
    {"return",  "return",  Keyword::Return,       true,  1},
    {"func",    "func",    Keyword::Func,         false, 0},
    {"main",    "main",    Keyword::Main,         false, 0},
    {"call",    "call",    Keyword::Call,         false, 0},
    {"log",     "log",     Keyword::Log,          true,  2},
    {"cth",     "cth",     Keyword::Cth,          true,  1},
    {"ctg",     "ctg",     Keyword::Ctg,          true,  1},
    {"sin",     "sin",     Keyword::Sin,          true,  1},
    {"cos",     "cos",     Keyword::Cos,          true,  1},
    {"if",      "if",      Keyword::If,           false, 0},
    {"ln",      "ln",      Keyword::Ln,           true,  1},
    {"tg",      "tg",      Keyword::Tg,           true,  1},
    {"sh",      "sh",      Keyword::Sh,           true,  1},
    {"ch",      "ch",      Keyword::Ch,           true,  1},
    {"th",      "th",      Keyword::Th,           true,  1},
    {":=",      ":=",      Keyword::Declarate,    false, 0},
    {"+",       "+",       Keyword::Add,          false, 2},
    {"-",       "-",       Keyword::Sub,          false, 2},
    {"*",       "*",       Keyword::Mul,          false, 2},
    {"/",       "/",       Keyword::Div,          false, 2},
    {"^",       "^",       Keyword::Pow,          false, 2},
    {"=",       "=",       Keyword::Assign,       false, 0},
    {";",       ";",       Keyword::Connect,      false, 0},
    {",",       ",",       Keyword::Comma,        false, 0},
    {"(",       "(",       Keyword::OpenParens,   false, 0},
    {")",       ")",       Keyword::CloseParens,  false, 0},
    {"{",       "{",       Keyword::OpenBracket,  false, 0},
    {"}",       "}",       Keyword::CloseBracket, false, 0},
};

static const size_t kKeywordsCount = sizeof(kKeywords) / sizeof(kKeywords[0]);

inline const KeywordInfo* FindKeywordInfo(Keyword kw) noexcept
{
    for (size_t i = 0; i < kKeywordsCount; ++i)
        if (kKeywords[i].kw == kw)
            return &kKeywords[i];
    return nullptr;
}

inline const KeywordInfo* FindBuiltinFunction(Keyword kw) noexcept
{
    for (size_t i = 0; i < kKeywordsCount; ++i)
        if (kKeywords[i].kw == kw && kKeywords[i].isFunction)
            return &kKeywords[i];
    return nullptr;
}

struct Token
{
    NodeType   type  = NodeType::Unknown;
    NodeValue  value = {};
    size_t     line  = 1;
    size_t     col   = 1;
};
