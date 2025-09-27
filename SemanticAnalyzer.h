#pragma once

#include "AST.h"
#include <llvm/IR/IRBuilder.h>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Support/MemoryBuffer.h>
#include <set>

class SemanticAnalyzer {
public:
    SemanticAnalyzer(std::unique_ptr<AST> ast) : ast(std::move(ast)) {
        using namespace std;
        AST std_ast(std::make_unique<Lex>("E:\\ZiYue4D\\out\\build\\x64-debug\\std.sb"));
        if (std_ast.parse()) throw semantic_exception("invalid standard library");
        for (auto& std_func : std_ast.extern_function_table)
        {
            std_func.second->name = "_ziyue4d_"s + std_func.second->name;
            this->ast->extern_function_table.emplace(std_func.first, std::move(std_func.second));
        }
    }
    bool analyze();

private:
    bool can_convert_to(SymbolType old_type, SymbolType new_type);
    const std::unique_ptr<FunctionSignatureAST>* seek_best_match_function(const CallExprAST& expr);
    SymbolType get_type(const std::unique_ptr<ExprAST>& expr);
    SymbolType llvm_type_to_symbol_type(llvm::Type* value);
    std::string readable_function_signature(const std::unique_ptr<FunctionSignatureAST>& signature);
    std::string readable_function_signature(const std::unique_ptr<FunctionAST>& signature);

    std::unique_ptr<AST> ast;
    std::unique_ptr<FunctionSignatureAST>* scope = nullptr;

    friend class CodeGen;
};