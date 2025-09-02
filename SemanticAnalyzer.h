#pragma once

#include "AST.h"
#include <llvm/IR/IRBuilder.h>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Support/MemoryBuffer.h>

class SemanticAnalyzer {
public:
    SemanticAnalyzer(std::unique_ptr<AST> ast) : ast(std::move(ast)) {
        llvm::LLVMContext dummy_context{};
        auto stdlibBuffer = llvm::MemoryBuffer::getFile("stdlib.bc");
        auto module = llvm::parseBitcodeFile(**stdlibBuffer, dummy_context);
        auto& functions = module.get()->getFunctionList();
        for (const auto& func : functions) {
            if (!func.getName().starts_with("_ziyue4d_")) continue;
            auto signature = std::make_unique<FunctionSignatureAST>(func.getName().str(), llvm_type_to_symbol_type(func.getReturnType()));
            for (size_t i = 0; i < func.arg_size(); i++)
            {
                signature->arguments.push_back(std::move(std::make_unique<FunctionArgument>(func.getArg(i)->getName().str(), llvm_type_to_symbol_type(func.getArg(i)->getType()), nullptr)));
            }
            this->ast->extern_function_table.insert({ func.getName().substr(9).str(), std::move(signature) });
        }
    }
    void analyze();

private:
    bool can_convert_to(SymbolType old_type, SymbolType new_type);
    const std::unique_ptr<FunctionSignatureAST>& seek_best_match_function(const CallExprAST& expr);
    SymbolType get_type(const std::unique_ptr<ExprAST>& expr);
    SymbolType llvm_type_to_symbol_type(llvm::Type* value);
    std::string readable_function_signature(const std::unique_ptr<FunctionSignatureAST>& signature);
    std::string readable_function_signature(const std::unique_ptr<FunctionAST>& signature);

    std::unique_ptr<AST> ast;
    std::unique_ptr<FunctionSignatureAST>* scope = nullptr;

    friend class CodeGen;
};