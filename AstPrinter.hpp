#pragma once

#include <fstream>
#include <string>
#include <stdexcept>

#include "AstNode.hpp"
#include "Tokenizer.hpp" 

class AstPrinter
{
public:
    void saveToFile(const std::string& fileName,
                    const AstNode&     root,
                    const NamesTable&  names)
    {
        std::ofstream f(fileName);
        if (!f)
            throw std::runtime_error("Cannot open file for writing: " + fileName);

        printNode(f, root, names, 0);
        f << '\n';
    }

private:
    void indent(std::ofstream& f, int depth)
    {
        for (int i = 0; i < depth; ++i)
            f << '\t';
    }

    void printNodeValue(std::ofstream& f,
                        const AstNode& n,
                        const NamesTable& names)
    {
        switch (n.type)
        {
            case NodeType::Unknown:  f << "UNKNOWN"; break;
            case NodeType::ConstNum: f << n.value.number; break;
            case NodeType::Keyword:
            {
                const auto* ki = FindKeywordInfo(n.keyword());
                f << (ki ? ki->standardName : "?");
                break;
            }
            case NodeType::Name:
            {
                const auto* e = names.findByIdx(n.value.idx);
                if (e) f << '"' << e->name << '"';
                else   f << "\"?\"";
                break;
            }
        }
    }

    void printNode(std::ofstream& f,
                   const AstNode& n,
                   const NamesTable& names,
                   int depth)
    {
        f << "( ";
        printNodeValue(f, n, names);
        f << '\n';

        indent(f, depth + 1);
        if (n.left)  printNode(f, *n.left,  names, depth + 1);
        else         f << "nil";

        f << '\n';
        indent(f, depth + 1);
        if (n.right) printNode(f, *n.right, names, depth + 1);
        else         f << "nil";

        f << '\n';
        indent(f, depth);
        f << ')';
    }
};
