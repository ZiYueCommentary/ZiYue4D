#pragma once

#include "AST.h"

class SemanticAnalyzer {
public:
    SemanticAnalyzer(std::unique_ptr<AST> ast) : ast(std::move(ast)) {}
    void analyze();

private:
    bool can_convert_to(SymbolType old_type, SymbolType new_type);
    const std::unique_ptr<FunctionSignatureAST>& seek_best_match_function(const CallExprAST& expr);
    SymbolType get_type(const std::unique_ptr<ExprAST>& expr);
    std::string readable_function_signature(const std::unique_ptr<FunctionSignatureAST>& signature);
    std::string readable_function_signature(const std::unique_ptr<FunctionAST>& signature);
    std::unique_ptr<AST> ast;
    std::unique_ptr<FunctionSignatureAST>* scope = nullptr;

    friend class CodeGen;
};