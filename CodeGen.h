#pragma once

#include <set>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include "SemanticAnalyzer.h"

struct Lifecycle {
    bool is_function;
    std::set<llvm::Value*> values;
};

class CodeGen {
public:
    CodeGen(std::unique_ptr<SemanticAnalyzer> semantic) : semantic(std::move(semantic)) {
        this->context = std::make_unique<llvm::LLVMContext>();
        this->builder = std::make_unique<llvm::IRBuilder<>>(*context);
        this->module = std::make_unique<llvm::Module>("ziyue4d", *context);
    }

    virtual ~CodeGen() = default;

    void optimize_string();

    bool generate();

private:
    llvm::Value* visit(const std::unique_ptr<ExprAST>& expr);

    llvm::Value* cast_value_to(llvm::Value* value, SymbolType type);

    llvm::FunctionType* create_function_type(const std::unique_ptr<FunctionSignatureAST>& signature);

    [[nodiscard]] llvm::Type* token_to_type(Token token) const;

    [[nodiscard]] llvm::Type* symbol_type_to_type(SymbolType type) const;

    std::string unique_function_name(const std::unique_ptr<FunctionSignatureAST>& signature);

    void update_variable_value(const std::string& name, llvm::Value* value);

    llvm::Value* find_variable_value(const std::string& name);

    void release_lifecycle_resources(bool is_function_return = false, const llvm::Value* string_return_value = nullptr);
    /// Build literal string data to LLVM IR.
    llvm::Value* build_literal_string(const std::string& str);
    /// Try to merge string operations that can be evaluated during compilation.
    std::unique_ptr<ExprAST> merge_literal_string_operations(std::unique_ptr<ExprAST> expr);
    /// Returns whether the expression can be evaluated during compilation.
    bool is_literal_expression(const ExprAST& expr);

    [[nodiscard]] bool is_non_string_literal_value(const std::unique_ptr<ExprAST>& expr) const;

    std::string literal_to_string(const ExprAST& expr);

    void build_scoped_symbol_table(const SymbolTable& symbol_table);

    static std::string to_lower_string(const std::string& str);

    std::unique_ptr<llvm::LLVMContext> context;
    std::unique_ptr<llvm::IRBuilder<>> builder;
    std::unique_ptr<llvm::Module> module;
    std::vector<std::unordered_map<std::string, llvm::Value*>> scoped_symbol_table = {{}};
    std::vector<Lifecycle> lifecycles;
    std::unique_ptr<SemanticAnalyzer> semantic;

    friend class Compiler;
};

class Compiler : public CodeGen {
public:
    Compiler(std::unique_ptr<SemanticAnalyzer> semantic) : CodeGen(std::move(semantic)) {
    }

    std::error_code write_file(const std::string& file);
};
