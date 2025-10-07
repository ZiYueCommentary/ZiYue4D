
#include "CodeGen.h"
#include "CLI11.hpp"
#include <iostream>

int main(int argc, char** argv) {
    CLI::App app{ "ZiYue4D Compiler" };

    // Define options
    std::string input, output;
    app.add_option("-i,--input", input, "Input")->required(true);
    app.add_option("-o,--output", output, "Input")->required(true);

    CLI11_PARSE(app, argc, argv);
     
    std::system("chcp 65001");
    std::cout << "Compiling...\n";
    AST ast(std::make_unique<Lex>(input));
    if (ast.parse()) return 1;
    std::cout << "Analyzing...\n";
    SemanticAnalyzer analyzer(std::make_unique<AST>(std::move(ast)));
    if (analyzer.analyze()) return 1;
    std::cout << "Generating...\n";
    Compiler codegen(std::make_unique<SemanticAnalyzer>(std::move(analyzer)));
    codegen.optimize_string();
    codegen.generate();

    codegen.write_file(output);
    //std::cout << "Executing...\n";
    //codegen.init();
    //std::cout << codegen.run();
    return 0;
}
