//читает AST в префиксной нотации, генерирует ассемблер

//запуск:  ./ast_to_asm input.ast [output.asm]

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <stdexcept>
#include <cassert>
#include <cctype>
#include <cstdlib>

//1.типы и ключевые слова

enum NodeType
{
    TYPE_UNKNOWN  = 0,
    TYPE_CONST_NUM,
    TYPE_KEYWORD,
    TYPE_VARIABLE,
    TYPE_NAME,
};

enum KeywordIdx
{
    KEY_UNKNOWN  = 0,
    KEY_ADD,
    KEY_SUB,
    KEY_MUL,
    KEY_DIV,
    KEY_POW,
    KEY_LOG,
    KEY_LN,
    KEY_SIN,
    KEY_COS,
    KEY_TG,
    KEY_CTG,
    KEY_ARCSIN,
    KEY_ARCCOS,
    KEY_ARCTG,
    KEY_ARCCTG,
    KEY_SH,
    KEY_CH,
    KEY_TH,
    KEY_CTH,
    KEY_INPUT,
    KEY_PRINT,
    KEY_CONNECT,
    KEY_COMMA,
    KEY_DECLARATE,
    KEY_ASSIGN,
    KEY_IF,
    KEY_OPEN_PARENS,
    KEY_CLOSE_PARENS,
    KEY_OPEN_BRACKET,
    KEY_CLOSE_BRACKET,
    KEY_FUNC,
    KEY_MAIN,
    KEY_RETURN,
    KEY_CALL,
};

struct KeywordInfo
{
    const char* asmName;     //имя в файле AST
    const char* stdName;     //«стандартное» имя для отладки
    KeywordIdx  idx;
    bool        isFunction;
    int         numberOfArgs;
};

//ключевые слова из ast-файла
static const KeywordInfo kKeywords[] =
{
    { "uknown",  "uknown",  KEY_UNKNOWN,       false, 0 },
    { "+",       "+",       KEY_ADD,            false, 2 },
    { "-",       "-",       KEY_SUB,            false, 2 },
    { "*",       "*",       KEY_MUL,            false, 2 },
    { "/",       "/",       KEY_DIV,            false, 2 },
    { "^",       "^",       KEY_POW,            false, 2 },
    { "log",     "log",     KEY_LOG,            true,  2 },
    { "ln",      "ln",      KEY_LN,             true,  1 },
    { "sin",     "sin",     KEY_SIN,            true,  1 },
    { "cos",     "cos",     KEY_COS,            true,  1 },
    { "tg",      "tg",      KEY_TG,             true,  1 },
    { "ctg",     "ctg",     KEY_CTG,            true,  1 },
    { "arcsin",  "arcsin",  KEY_ARCSIN,         true,  1 },
    { "arccos",  "arccos",  KEY_ARCCOS,         true,  1 },
    { "arctg",   "arctg",   KEY_ARCTG,          true,  1 },
    { "arcctg",  "arcctg",  KEY_ARCCTG,         true,  1 },
    { "sh",      "sh",      KEY_SH,             true,  1 },
    { "ch",      "ch",      KEY_CH,             true,  1 },
    { "th",      "th",      KEY_TH,             true,  1 },
    { "cth",     "cth",     KEY_CTH,            true,  1 },
    { "input",   "input",   KEY_INPUT,          true,  0 },
    { "print",   "print",   KEY_PRINT,          true,  1 },
    { ";",       ";",       KEY_CONNECT,        false, 0 },
    { ",",       ",",       KEY_COMMA,          false, 0 },
    { ":=",      ":=",      KEY_DECLARATE,      false, 0 },
    { "=",       "=",       KEY_ASSIGN,         false, 0 },
    { "if",      "if",      KEY_IF,             false, 0 },
    { "(",       "(",       KEY_OPEN_PARENS,    false, 0 },
    { ")",       ")",       KEY_CLOSE_PARENS,   false, 0 },
    { "{",       "{",       KEY_OPEN_BRACKET,   false, 0 },
    { "}",       "}",       KEY_CLOSE_BRACKET,  false, 0 },
    { "func",    "func",    KEY_FUNC,           false, 0 },
    { "main",    "main",    KEY_MAIN,           false, 0 },
    { "return",  "return",  KEY_RETURN,         true,  1 },
    { "call",    "call",    KEY_CALL,           false, 0 },
};
static const size_t kNumKeywords = sizeof(kKeywords) / sizeof(KeywordInfo);

//Поиск слова по строке из AST
static const KeywordInfo* FindKeywordByName(const std::string& name)
{
    for (size_t i = 0; i < kNumKeywords; ++i)
        if (name == kKeywords[i].asmName)
            return &kKeywords[i];
    return nullptr;
}

static const KeywordInfo* FindKeywordByIdx(KeywordIdx idx)
{
    for (size_t i = 0; i < kNumKeywords; ++i)
        if (kKeywords[i].idx == idx)
            return &kKeywords[i];
    return nullptr;
}

//2.узел ast

struct Node
{
    NodeType type     = TYPE_UNKNOWN;
    int      number   = 0;      //значение для TYPE_CONST_NUM
    size_t   idx      = 0;      //для TYPE_KEYWORD  – KeywordIdx
                                 //для TYPE_VARIABLE/TYPE_NAME – индекс в таблице имён

    Node* left  = nullptr;
    Node* right = nullptr;

    ~Node()
    {
        delete left;
        delete right;
    }
};

//3.таблица имен

class NamesTable
{
public:
    //Добавляет имя если не существует и возвращает индекс
    size_t findOrAdd(const std::string& name)
    {
        auto it = index_.find(name);
        if (it != index_.end())
            return it->second;
        size_t idx = names_.size();
        names_.push_back(name);
        index_[name] = idx;
        return idx;
    }

    const std::string& get(size_t idx) const
    {
        if (idx >= names_.size())
            throw std::runtime_error("NamesTable: index out of range: " + std::to_string(idx));
        return names_[idx];
    }

    size_t size() const { return names_.size(); }

private:
    std::vector<std::string>              names_;
    std::unordered_map<std::string, size_t> index_;
};

//4.парсер AST-файла

class AstParser
{
public:
    AstParser(const std::string& source, NamesTable& names)
        : src_(source), pos_(0), names_(names) {}

    Node* parse()
    {
        skipSpaces();
        return parseNode();
    }

private:
    const std::string& src_;
    size_t             pos_;
    NamesTable&        names_;


    void skipSpaces()
    {
        while (pos_ < src_.size() && std::isspace((unsigned char)src_[pos_]))
            ++pos_;
    }

    char peek() const
    {
        return (pos_ < src_.size()) ? src_[pos_] : '\0';
    }

    char consume()
    {
        return src_[pos_++];
    }

    bool tryConsume(const std::string& s)
    {
        if (src_.compare(pos_, s.size(), s) == 0)
        {
            pos_ += s.size();
            return true;
        }
        return false;
    }

    // Читаем последовательность непробельных, нескобочных символов, двойные ковычки обрабываем отдельно
    std::string readToken()
    {
        std::string tok;
        while (pos_ < src_.size())
        {
            char c = src_[pos_];
            if (std::isspace((unsigned char)c) || c == '(' || c == ')')
                break;
            tok += c;
            ++pos_;
        }
        return tok;
    }

    //рекурсивный парсинг

    //Возвращает новый узел или nullptr
    Node* parseNode()
    {
        skipSpaces();

        if (pos_ >= src_.size())
            throw std::runtime_error("Unexpected end of input");

        if (peek() == '(')
        {
            consume(); // '('
            skipSpaces();

            Node* node = parseNodeContents();

            skipSpaces();
            if (peek() != ')')
                throw std::runtime_error(
                    std::string("Expected ')' at pos ") + std::to_string(pos_) +
                    ", got '" + peek() + "'");
            consume(); // ')'
            return node;
        }
        else if (src_.compare(pos_, 3, "nil") == 0)
        {
            pos_ += 3;
            return nullptr;
        }
        else
        {
            throw std::runtime_error(
                std::string("Unexpected char '") + peek() +
                "' at pos " + std::to_string(pos_));
        }
    }

    //Парсит содержимое скобок: тип+значение, затем левый и правый дочерние узлы
    Node* parseNodeContents()
    {
        Node* node = new Node();

        //Определяем тип и значение текущего узла

        if (peek() == '"')
        {
            //Строковое имя переменной/функции
            consume(); // '"'
            std::string name;
            while (pos_ < src_.size() && src_[pos_] != '"')
                name += src_[pos_++];
            if (pos_ >= src_.size())
            {
                delete node;
                throw std::runtime_error("Unterminated string literal");
            }
            consume(); // '"'

            size_t idx = names_.findOrAdd(name);
            node->type = TYPE_VARIABLE;
            node->idx  = idx;
        }
        else
        {
            std::string tok = readToken();
            if (tok.empty())
            {
                delete node;
                throw std::runtime_error(
                    std::string("Empty token at pos ") + std::to_string(pos_));
            }

            //Пробуем распознать как целое число
            bool isNumber = false;
            {
                size_t i = 0;
                if (tok[i] == '-' || tok[i] == '+') ++i;
                if (i < tok.size())
                {
                    isNumber = true;
                    for (; i < tok.size(); ++i)
                        if (!std::isdigit((unsigned char)tok[i]))
                        { isNumber = false; break; }
                }
            }

            if (isNumber)
            {
                node->type   = TYPE_CONST_NUM;
                node->number = std::stoi(tok);
            }
            else
            {
                //поиск ключевое слово
                const KeywordInfo* kw = FindKeywordByName(tok);
                if (kw != nullptr)
                {
                    node->type = TYPE_KEYWORD;
                    node->idx  = (size_t)kw->idx;
                }
                else
                {
                    //Неизвестный токен – сохраняем как имя
                    std::cerr << "[WARNING] Unknown token \"" << tok
                              << "\" at pos " << pos_ << ", treating as name\n";
                    size_t idx = names_.findOrAdd(tok);
                    node->type = TYPE_VARIABLE;
                    node->idx  = idx;
                }
            }
        }

        //2.Левый и правый дочерние узлы
        skipSpaces();
        node->left = parseNode();
        skipSpaces();
        node->right = parseNode();

        return node;
    }
};

//5.генератор ассемблера

class AsmGenerator
{
public:
    AsmGenerator(const NamesTable& names, std::ostream& out)
        : names_(names), out_(out), ifCounter_(0) {}

    void generate(Node* root)
    {
        out_ << "; Compiled from rap language (ast_to_asm)\n\n";
        out_ << "CALL :main\n";
        out_ << "HLT\n";
        assembleNode(root);
    }

private:
    const NamesTable& names_;
    std::ostream&     out_;
    size_t            ifCounter_;


    const std::string& getName(size_t idx) const
    {
        return names_.get(idx);
    }


    void assembleNode(Node* node)
    {
        if (node == nullptr)
            throw std::runtime_error("assembleNode: got nullptr");

        switch (node->type)
        {
            case TYPE_UNKNOWN:
                throw std::runtime_error("assembleNode: TYPE_UNKNOWN node");

            case TYPE_CONST_NUM:
                out_ << "PUSH " << node->number << "\n";
                break;

            case TYPE_VARIABLE:
                //читаем переменную из памяти по её индексу
                out_ << "PUSH "   << node->idx << "\n"
                     << "POPR RAX\n"
                     << "PUSHM [RAX]\n\n";
                break;

            case TYPE_NAME:
                throw std::runtime_error(
                    "assembleNode: TYPE_NAME node should not be assembled directly");

            case TYPE_KEYWORD:
                assembleKeyword(node);
                break;

            default:
                throw std::runtime_error("assembleNode: unknown node type");
        }
    }

    void assembleKeyword(Node* node)
    {
        assert(node->type == TYPE_KEYWORD);
        auto kwIdx = static_cast<KeywordIdx>(node->idx);
        const KeywordInfo* kw = FindKeywordByIdx(kwIdx);

        //вычисляем аргументы (левый, затем правый),
        //если ключевое слово имеет numberOfArgs > 0
        if (kw && kw->numberOfArgs >= 1 && node->left)
            assembleNode(node->left);
        if (kw && kw->numberOfArgs == 2 && node->right)
            assembleNode(node->right);

        switch (kwIdx)
        {
            case KEY_ADD:
                out_ << "ADD\n\n";
                break;

            case KEY_SUB:
                out_ << "SUB\n\n";
                break;

            case KEY_MUL:
                out_ << "MUL\n\n";
                break;

            case KEY_DIV:
                out_ << "DIV\n\n";
                break;

            case KEY_POW:
                out_ << "; POW (not implemented in VM, stub)\n"
                     << "CALL :__pow\n\n";
                break;

            case KEY_LOG:
            case KEY_LN:
            case KEY_SIN:
            case KEY_COS:
            case KEY_TG:
            case KEY_CTG:
            case KEY_ARCSIN:
            case KEY_ARCCOS:
            case KEY_ARCTG:
            case KEY_ARCCTG:
            case KEY_SH:
            case KEY_CH:
            case KEY_TH:
            case KEY_CTH:
            {
                const char* mathName = kw ? kw->asmName : "math_fn";
                out_ << "; Math function: " << mathName << "\n"
                     << "CALL :__" << mathName << "\n\n";
                break;
            }

            case KEY_INPUT:
                out_ << "IN\n";
                break;

            case KEY_PRINT:
                out_ << "OUT\n";
                break;

            case KEY_CONNECT:
                if (node->left)
                    assembleNode(node->left);
                if (node->right)
                    assembleNode(node->right);
                break;

            case KEY_IF:
                assembleIf(node);
                break;

            //присваивание/объявление
            case KEY_DECLARATE:
            case KEY_ASSIGN:
            {
                //вычисляем правое поддерево
                if (!node->right)
                    throw std::runtime_error("ASSIGN/DECLARATE: missing right child");
                assembleNode(node->right);

                if (!node->left || node->left->type != TYPE_VARIABLE)
                    throw std::runtime_error("ASSIGN/DECLARATE: left child must be TYPE_VARIABLE");

                size_t varIdx = node->left->idx;
                out_ << "PUSH "   << varIdx << "\n"
                     << "POPR RAX\n"
                     << "POPM [RAX]\n\n";
                break;
            }

            case KEY_FUNC:
                assembleFunc(node);
                break;

            case KEY_MAIN:
                assembleMain(node);
                break;

            case KEY_RETURN:
            {
                if (node->left)
                    assembleNode(node->left);
                else
                    out_ << "; return без значения\n";

                out_ << "POPR RAX\n"
                     << "RET\n\n";
                break;
            }

            case KEY_CALL:
                assembleCall(node);
                break;

            case KEY_COMMA:
                std::cerr << "[WARNING] KEY_COMMA in AST – ignoring\n";
                break;

            case KEY_UNKNOWN:
                throw std::runtime_error("assembleKeyword: KEY_UNKNOWN");

            default:
                throw std::runtime_error(
                    std::string("assembleKeyword: unhandled keyword idx=") +
                    std::to_string(kwIdx));
        }
    }

    //if
    void assembleIf(Node* node)
    {
        size_t label = ifCounter_++;

        out_ << "; if #" << label << "\n";

        //Вычисляем условие
        if (!node->left)
            throw std::runtime_error("IF: missing condition (left child)");
        assembleNode(node->left);

        out_ << "PUSH 0\n"
             << "JE :endif_" << label << "\n";

        //Тело
        if (!node->right)
            throw std::runtime_error("IF: missing body (right child)");
        assembleNode(node->right);

        out_ << ":endif_" << label << "\n\n";
    }

    //объявление функции
    void assembleFunc(Node* node)
    {

        Node* args = node->left;
        if (!args)
            throw std::runtime_error("FUNC: missing arguments node");

        Node* funcNameNode = args->left;
        if (!funcNameNode)
            throw std::runtime_error("FUNC: missing function name node");

        const std::string& funcName = getName(funcNameNode->idx);
        out_ << "\n:" << funcName << "\n";

        if (args->right)
            assembleParams(args->right);

        //Тело
        Node* body = node->right;
        if (!body)
            throw std::runtime_error("FUNC: missing body");
        assembleNode(body);
    }

    //обход параметров функции
    void assembleParams(Node* node)
    {
        if (!node) return;
        if (node->type == TYPE_VARIABLE)
        {
            out_ << "; param " << getName(node->idx) << "\n"
                 << "PUSH "   << node->idx << "\n"
                 << "POPR RAX\n"
                 << "POPM [RAX]\n";
            return;
        }
        if (node->type == TYPE_KEYWORD &&
            static_cast<KeywordIdx>(node->idx) == KEY_COMMA)
        {
            assembleParams(node->left);
            assembleParams(node->right);
            return;
        }
        assembleParams(node->left);
        assembleParams(node->right);
    }

    //main
    void assembleMain(Node* node)
    {
        out_ << "\n:main\n";

        Node* body = node->right;
        if (!body)
            throw std::runtime_error("MAIN: missing body");
        assembleNode(body);
    }

    //вызов функции
    void assembleCall(Node* node)
    {
        Node* funcNameNode = node->left;
        if (!funcNameNode)
            throw std::runtime_error("CALL: missing function name");

        const std::string& funcName = getName(funcNameNode->idx);

        //Аргументы, если есть
        if (node->right)
            assembleNode(node->right);

        out_ << "\nCALL :" << funcName << "\n"
             << "PUSHR RAX\n\n";
    }
};

// 6.чтение файла в строчку

static std::string readFile(const std::string& path)
{
    std::ifstream f(path);
    if (!f.is_open())
        throw std::runtime_error("Cannot open file: " + path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

//7.main

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <input.ast> [output.asm]\n";
        return 1;
    }

    const std::string inputFile  = argv[1];
    const std::string outputFile = (argc >= 3) ? argv[2] : "output.asm";

    //Читаем исходный AST-файл
    std::string source;
    try
    {
        source = readFile(inputFile);
    }
    catch (const std::exception& e)
    {
        std::cerr << "[ERROR] " << e.what() << "\n";
        return 1;
    }

    //Парсим AST
    NamesTable names;
    Node* root = nullptr;

    try
    {
        AstParser parser(source, names);
        root = parser.parse();
    }
    catch (const std::exception& e)
    {
        std::cerr << "[ERROR] Parse error: " << e.what() << "\n";
        return 1;
    }

    //Генерируем ассемблер
    std::ofstream outStream(outputFile);
    if (!outStream.is_open())
    {
        std::cerr << "[ERROR] Cannot open output file: " << outputFile << "\n";
        delete root;
        return 1;
    }

    try
    {
        AsmGenerator gen(names, outStream);
        gen.generate(root);
    }
    catch (const std::exception& e)
    {
        std::cerr << "[ERROR] Codegen error: " << e.what() << "\n";
        delete root;
        return 1;
    }

    delete root;

    std::cout << "Compiled \"" << inputFile << "\" -> \"" << outputFile << "\"\n";
    return 0;
}
