#pragma once

#include "AST.h"
#include <llvm/IR/IRBuilder.h>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Support/MemoryBuffer.h>
#include <set>

class SemanticAnalyzer {
public:
    SemanticAnalyzer(std::unique_ptr<AST> ast) : ast(std::move(ast)) {
        llvm::LLVMContext dummy_context{};
        auto stdlibBuffer = llvm::MemoryBuffer::getFile("stdlib.bc");
        auto module = llvm::parseBitcodeFile(**stdlibBuffer, dummy_context);
        llvm::GlobalVariable* annotations = module->get()->getGlobalVariable("llvm.global.annotations");
        std::set<std::string> return_string_functions = {};
        llvm::ConstantArray* anootation_array = llvm::dyn_cast<llvm::ConstantArray>(annotations->getInitializer());
        for (unsigned i = 0; i < anootation_array->getNumOperands(); ++i) {
            llvm::ConstantStruct* constant = dyn_cast<llvm::ConstantStruct>(anootation_array->getOperand(i));
            if (!constant) continue;
            llvm::Value* annotated_entity = constant->getOperand(0)->stripPointerCasts();
            llvm::GlobalVariable* annotation_variable = dyn_cast<llvm::GlobalVariable>(constant->getOperand(1)->stripPointerCasts());
            if (!annotation_variable) continue;
            llvm::ConstantDataArray* annotation_data_array = dyn_cast<llvm::ConstantDataArray>(annotation_variable->getInitializer());
            if (!annotation_data_array || !annotation_data_array->isString()) continue;

            llvm::StringRef annotation_string = annotation_data_array->getAsString();
            if (annotation_string == "ziyue4d_string") return_string_functions.insert(anootation_array->getName().str());
        }

        auto& functions = module.get()->getFunctionList();
        for (const auto& func : functions) {
            if (!func.getName().starts_with("_ziyue4d_")) continue;
            auto signature = std::make_unique<FunctionSignatureAST>(
                func.getName().str(),
                return_string_functions.contains(func.getName().str()) ? SYMBOL_TYPE_STRING : llvm_type_to_symbol_type(func.getReturnType())
            );
            for (size_t i = 0; i < func.arg_size(); i++)
            {
                signature->arguments.push_back(std::move(std::make_unique<FunctionArgument>(
                    func.getArg(i)->getName().str(),
                    llvm_type_to_symbol_type(func.getArg(i)->getType()),
                    nullptr
                )));
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