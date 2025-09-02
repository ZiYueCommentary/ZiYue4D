#pragma once

#include "Lex.h"

using SymbolTable = std::unordered_multimap<std::string, SymbolType>;

class ExprAST {
public:
    virtual ~ExprAST() {}
};

struct FunctionArgument {
public:
    const std::string name;
    const SymbolType type;
    const std::unique_ptr<ExprAST> default_value;

    FunctionArgument(std::string&& name, SymbolType type, std::unique_ptr<ExprAST> default_value) : name(std::move(name)), type(type), default_value(std::move(default_value)) {}
    ~FunctionArgument() {}
};

class CallExprAST : public ExprAST {
public:
    CallExprAST(std::string&& name, std::vector<std::unique_ptr<ExprAST>>&& arguments) : name(std::move(name)), arguments(std::move(arguments)) {}

private:
    std::string name;
    std::vector<std::unique_ptr<ExprAST>> arguments;

    friend class SemanticAnalyzer;
    friend class CodeGen;
};

class VariableExprAST : public ExprAST {
public:
    VariableExprAST(std::string&& name) : name(name) {}

private:
    std::string name;

    friend class SemanticAnalyzer;
    friend class CodeGen;
};

class IntegerExprAST : public ExprAST {
public:
    IntegerExprAST(int value) : value(value) {

    }

private:
    const int value;

    friend class CodeGen;
};

class FloatExprAST : public ExprAST {
public:
    FloatExprAST(float value) : value(value) {

    }

private:
    const float value;

    friend class CodeGen;
};

class StringExprAST : public ExprAST {
public:
    StringExprAST(std::string&& string) : string(string) {

    }

private:
    const std::string string;

    friend class CodeGen;
};

class ReturnExprAST : public ExprAST {
public:
    ReturnExprAST(std::unique_ptr<ExprAST> expr) : expr(std::move(expr)) {}

private:
    std::unique_ptr<ExprAST> expr;

    friend class SemanticAnalyzer;
    friend class CodeGen;
};

class UnaryExprAST : public ExprAST {
public:
    UnaryExprAST(int op, std::unique_ptr<ExprAST> expr) : op(op), expr(std::move(expr)) {}

private:
    int op;
    std::unique_ptr<ExprAST> expr;

    friend class SemanticAnalyzer;
    friend class CodeGen;
};

class BinaryExprAST : public ExprAST {
public:
    BinaryExprAST(int op, std::unique_ptr<ExprAST> lhs, std::unique_ptr<ExprAST> rhs) : op(op), lhs(std::move(lhs)), rhs(std::move(rhs)) {

    }

private:
    int op;
    std::unique_ptr<ExprAST> lhs;
    std::unique_ptr<ExprAST> rhs;

    friend class SemanticAnalyzer;
    friend class CodeGen;
};

class FunctionSignatureAST : public ExprAST {
public:
    FunctionSignatureAST(std::string name, SymbolType return_value_type) : name(name), return_value_type(return_value_type) {
        this->symbol_table = {};
    }

    std::string name;
    SymbolType return_value_type;
    std::vector<std::unique_ptr<FunctionArgument>> arguments;
    SymbolTable symbol_table;

    friend class SemanticAnalyzer;
    friend class CodeGen;
};

class FunctionAST : public ExprAST {
public:
    FunctionAST(std::unique_ptr<FunctionSignatureAST> signature) : signature(std::move(signature)) {
    }

    std::unique_ptr<FunctionSignatureAST> signature;
    std::vector<std::unique_ptr<ExprAST>> body;

    friend class SemanticAnalyzer;
    friend class CodeGen;
};

using FunctionTable = std::unordered_multimap<std::string, std::unique_ptr<FunctionAST>>;
using ExternFunctionTable = std::unordered_map<std::string, std::unique_ptr<FunctionSignatureAST>>;

class AST {
public:
    AST(std::unique_ptr<Lex> lex) : lex(std::move(lex)) {}

    void parse();

private:
    std::unique_ptr<ExprAST> parse_expression(std::unique_ptr<ExprAST> lhs, SymbolTable& symbol_table, bool function_first = true);
    std::unique_ptr<ExprAST> parse_primary_expression(SymbolTable& symbol_table, bool function_first = true);
    std::unique_ptr<CallExprAST> parse_call_expression(const std::string callee, SymbolTable& symbol_table);
    std::unique_ptr<FunctionSignatureAST> parse_function_signature();
    void parse_function_definition();
    int is_variable(SymbolTable& symbol_table, const std::string& name);

    std::unique_ptr<Lex> lex;
    SymbolTable global_symbols;
    FunctionTable function_table;
    ExternFunctionTable extern_function_table;
    int token = 0;

    friend class SemanticAnalyzer;
    friend class CodeGen;
};