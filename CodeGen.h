#pragma once

#include "SemanticAnalyzer.h"
#pragma warning(push)
#pragma warning(disable: 4146 4996)
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/IR/IRBuilder.h>
#pragma warning(pop)
#include <stack>

struct StringLifecycle {
    bool is_function;
    std::set<llvm::Value*> strings;
};

class CodeGen {
public:
    CodeGen(std::unique_ptr<SemanticAnalyzer> semantic) : semantic(std::move(semantic)) {
        this->context = std::make_unique<llvm::LLVMContext>();
        this->builder = std::make_unique<llvm::IRBuilder<>>(*context);
        this->module = std::make_unique<llvm::Module>("ziyue4d", *context);
    }
    virtual ~CodeGen() {}
    llvm::Value* generate_functions();

private:
    llvm::Value* visit(const std::unique_ptr<ExprAST>& expr);
    llvm::Value* cast_value_to(llvm::Value* value, SymbolType type);
    llvm::FunctionType* create_function_type(const std::unique_ptr<FunctionSignatureAST>& signature);
    llvm::Type* token_to_type(Token token);
    llvm::Type* symbol_type_to_type(SymbolType type);
    std::string unique_function_name(const std::unique_ptr<FunctionSignatureAST>& signature);
    void update_variable_value(const std::string& name, llvm::Value* value);
    llvm::Value* find_variable_value(const std::string& name);
    void release_lifecycle_string(bool is_function_return = false, llvm::Value* string_return_value = nullptr);
    llvm::Value* build_literal_string(const std::string& str);

    std::unique_ptr<llvm::LLVMContext> context;
    std::unique_ptr<llvm::IRBuilder<>> builder;
    std::unique_ptr<llvm::Module> module;
    std::vector<std::unordered_map<std::string, llvm::Value*>> scoped_symbol_table = { {} };
    std::stack<StringLifecycle> string_lifecycles;
    std::unique_ptr<SemanticAnalyzer> semantic;

    friend class JIT;
};

class JIT : public CodeGen {
public:
    JIT(std::unique_ptr<SemanticAnalyzer> semantic) : CodeGen(std::move(semantic)) {}
    void init();
    int run();

private:
    std::unique_ptr<llvm::orc::LLJIT> jit;
};
