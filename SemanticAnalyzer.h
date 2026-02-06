#pragma once

#include "AST.h"

class SemanticAnalyzer {
public:
    SemanticAnalyzer(std::unique_ptr<AST> ast, const std::string& std) : ast(std::move(ast)) {
        using namespace std;
        AST std_ast(std::make_unique<Lex>(std));
        if (std_ast.parse()) throw semantic_exception("invalid standard library");
        // initializing constants from std...
        for (auto&[name, value] : std_ast.constant_table)
        {
            this->ast->scoped_symbol_table.front()->insert({ name, std_ast.scoped_symbol_table.front()->equal_range(name).first->second });
            this->ast->constant_table.emplace(name, std::move(value));
        }
        // initializing functions from std...
        for (auto&[name, signature] : std_ast.extern_function_table)
        {
            signature->name = "ziyue4d_"s + signature->name;
            this->ast->extern_function_table.emplace(name, std::move(signature));
        }
    }
    bool analyze();

private:
    bool can_convert_to(SymbolType old_type, SymbolType new_type);
    const std::unique_ptr<FunctionSignatureAST>* seek_best_match_function(const CallExprAST& expr);
    SymbolType get_type(const std::unique_ptr<ExprAST>& expr);
    bool is_constant_expression(const std::unique_ptr<ExprAST>& expr);
    bool is_relational_operator(int token);
    bool is_bitwise_or_logic_operator(int token);
    std::string readable_function_signature(const std::unique_ptr<FunctionSignatureAST>& signature);
    std::string readable_function_signature(const std::unique_ptr<FunctionAST>& signature);

    std::unique_ptr<AST> ast;
    std::unique_ptr<FunctionSignatureAST>* scope_function = nullptr;
    std::vector<std::unordered_map<std::string, SymbolType>> scoped_symbol_types;

    friend class CodeGen;
};