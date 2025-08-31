
#include "SemanticAnalyzer.h"

int main() {
    AST ast(std::make_unique<Lex>("E:\\ZiYue4D\\example.sb"));
    ast.parse();
    SemanticAnalyzer analyzer(std::make_unique<AST>(std::move(ast)));
    analyzer.analyze();
    return 0;
}