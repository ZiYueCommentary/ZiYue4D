
#include "CodeGen.h"
#include <iostream>

int main() {
    std::cout << "Compiling...\n";
    AST ast(std::make_unique<Lex>("E:\\ZiYue4D\\example.sb"));
    ast.parse();
    std::cout << "Analyzing...\n";
    SemanticAnalyzer analyzer(std::make_unique<AST>(std::move(ast)));
    analyzer.analyze();
    std::cout << "Generating...\n";
    JIT codegen(std::make_unique<SemanticAnalyzer>(std::move(analyzer)));
    codegen.generate_functions();
    codegen.init();
    //std::cout << codegen.run();
    return 0;
}