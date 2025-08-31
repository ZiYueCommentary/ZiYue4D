
#include "AST.h"

int main() {
    AST ast(std::make_unique<Lex>("E:\\ZiYue4D\\example.sb"));
    ast.parse();
    return 0;
}