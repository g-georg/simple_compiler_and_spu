#include <iostream>
#include "Tokenizer.hpp"
#include "Parser.hpp"
#include "AstPrinter.hpp"

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " source.rap\n";
        return 1;
    }

    try
    {
        NamesTable names;
        Tokenizer  tokenizer;

        auto tokens = tokenizer.tokenizeFile(argv[1], names);

        Parser  parser;
        NodePtr ast = parser.parse(std::move(tokens), names);

        AstPrinter printer;
        printer.saveToFile("ast_forest/tree.ast", *ast, names);

        std::cout << "AST saved to ast_forest/tree.ast\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
