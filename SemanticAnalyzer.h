#pragma once

#include "AST.h"
#include <llvm/IR/IRBuilder.h>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Support/MemoryBuffer.h>
#include <map>

class SemanticAnalyzer {
public:
    SemanticAnalyzer(std::unique_ptr<AST> ast, const std::string& std) : ast(std::move(ast)) {
        using namespace std;
        AST std_ast(std::make_unique<Lex>(std));
        if (std_ast.parse()) throw semantic_exception("invalid standard library");
        for (auto& constant : std_ast.constant_table)
        {
            this->ast->global_symbols.insert({ constant.first, std_ast.global_symbols.equal_range(constant.first).first->second });
            this->ast->constant_table.emplace(constant.first, std::move(constant.second));
        }
        for (auto& std_func : std_ast.extern_function_table)
        {
            std_func.second->name = "ziyue4d_"s + std_func.second->name;
            this->ast->extern_function_table.emplace(std_func.first, std::move(std_func.second));
        }
    }
    bool analyze();

private:
    bool can_convert_to(SymbolType old_type, SymbolType new_type);
    const std::unique_ptr<FunctionSignatureAST>* seek_best_match_function(const CallExprAST& expr);
    SymbolType get_type(const std::unique_ptr<ExprAST>& expr);
    SymbolType llvm_type_to_symbol_type(llvm::Type* value);
    bool is_constant_expression(const std::unique_ptr<ExprAST>& expr);
    bool is_relational_operator(int token);
    bool is_bitwise_or_logic_operator(int token);
    std::string readable_function_signature(const std::unique_ptr<FunctionSignatureAST>& signature);
    std::string readable_function_signature(const std::unique_ptr<FunctionAST>& signature);

    std::unique_ptr<AST> ast;
    std::unique_ptr<FunctionSignatureAST>* scope_function = nullptr;
    std::vector<std::unordered_map<std::string, SymbolType>> scoped_symbol_tables;

    friend class CodeGen;
};