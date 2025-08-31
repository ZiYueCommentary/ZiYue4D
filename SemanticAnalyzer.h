#pragma once

#include "AST.h"
#include <optional>

class SemanticAnalyzer {
public:
    SemanticAnalyzer(std::unique_ptr<AST> ast) : ast(std::move(ast)) {}
    void analyze();

private:
    std::optional<semantic_exception> unary_can_execute(const UnaryExprAST& expr);
    bool can_convert_to(SymbolType old_type, SymbolType new_type);
    SymbolType get_type(const std::unique_ptr<ExprAST>& expr);
    std::unique_ptr<AST> ast;
    SymbolTable* scope_symbol_table = nullptr;
};