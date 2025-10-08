
#include "CodeGen.h"
#include "CLI11.hpp"
#include <iostream>

int main(int argc, char** argv) {
    CLI::App app{ "ZiYue4D Compiler" };

    // Define options
    std::string input, output, std;
    bool show_progress = false, dump_module = false;
#ifdef _DEBUG
    input = "example.sb"; output = "main.ll"; std = "std.sb"; show_progress = true; dump_module = true;
#else
    CLI::Option* option_input = app.add_option("-i,--input", input, "Input source file")->check(CLI::ExistingFile)->required(true);
    CLI::Option* option_output = app.add_option("-o,--output", output, "Output source file")->required(true);
    CLI::Option* option_stdlib = app.add_option("-s,--std", std, "Standard header")->required(false)->default_val("std.sb");
    CLI::Option* option_progress = app.add_flag("-p,--progress", show_progress, "Show progress")->required(false);
    CLI::Option* option_dump = app.add_flag("-d,--dump", dump_module, "Dump output to the terminal")->required(false);
    
    CLI11_PARSE(app, argc, argv);
#endif

    if (show_progress) std::cout << "Compiling...\n";
    AST ast(std::make_unique<Lex>(input));
    if (ast.parse()) return 1;
    if (show_progress) std::cout << "Analyzing...\n";
    SemanticAnalyzer analyzer(std::make_unique<AST>(std::move(ast)), std);
    if (analyzer.analyze()) return 1;
    if (show_progress) std::cout << "Generating...\n";
    Compiler codegen(std::make_unique<SemanticAnalyzer>(std::move(analyzer)));
    codegen.optimize_string();
    codegen.generate();

    codegen.write_file(output, dump_module);
    //std::cout << "Executing...\n";
    //codegen.init();
    //std::cout << codegen.run();
    return 0;
}
