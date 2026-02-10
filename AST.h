#pragma once

#include <vector>
#include "Lex.h"

using SymbolTable = std::unordered_multimap<std::string, SymbolType>;

enum SymbolTableType {
    SYMBOL_TABLE_TYPE_GLOBAL,
    SYMBOL_TABLE_TYPE_FUNCTION,
    SYMBOL_TABLE_TYPE_IF,
    SYMBOL_TABLE_TYPE_WHILE
};

struct SymbolTableLayer {
    SymbolTable* symbol_table;
    SymbolTableType layer_type;
};

class ExprAST {
public:
    virtual ~ExprAST() = default;
};

using GlobalTable = std::unordered_map<std::string, std::unique_ptr<ExprAST>>;

struct FunctionArgument {
    const std::string name;
    const SymbolType type;
    const std::unique_ptr<ExprAST> default_value;

    FunctionArgument(std::string&& name, const SymbolType type, std::unique_ptr<ExprAST> default_value) : name(std::move(name)), type(type), default_value(std::move(default_value)) {}
    ~FunctionArgument() = default;
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
    FloatExprAST(const float value) : value(value) {

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
    FunctionSignatureAST(std::string name, const SymbolType return_value_type) : name(std::move(name)), return_value_type(return_value_type) {
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

class IfStatementAST : public ExprAST {
public:
    IfStatementAST(std::unique_ptr<ExprAST> condition) : condition(std::move(condition)) {
    }

    std::unique_ptr<ExprAST> condition;
    std::vector<std::unique_ptr<ExprAST>> statement_true;
    std::vector<std::unique_ptr<ExprAST>> statement_false;
    SymbolTable statement_true_symbol_table;
    SymbolTable statement_false_symbol_table;

    friend class SemanticAnalyzer;
    friend class CodeGen;
};

class WhileStatementAST : public ExprAST {
public:
    WhileStatementAST(std::unique_ptr<ExprAST> condition) : condition(std::move(condition)) {
    }

    std::unique_ptr<ExprAST> condition;
    std::vector<std::unique_ptr<ExprAST>> statement_true;
    SymbolTable statement_true_symbol_table;

    friend class SemanticAnalyzer;
    friend class CodeGen;
};

class ExitAST : public ExprAST {
public:
    ExitAST() {}

    friend class SemanticAnalyzer;
    friend class CodeGen;
};

class ContinueAST : public ExprAST {
public:
    ContinueAST() {}

    friend class SemanticAnalyzer;
    friend class CodeGen;
};

using FunctionTable = std::unordered_multimap<std::string, std::unique_ptr<FunctionAST>>;
using ExternFunctionTable = std::unordered_map<std::string, std::unique_ptr<FunctionSignatureAST>>;

class AST {
public:
    AST(std::unique_ptr<Lex> lex) : lex(std::move(lex)) {}

    bool parse();

private:
    std::unique_ptr<ExprAST> parse_expression(std::unique_ptr<ExprAST> lhs, bool function_first = true);
    std::unique_ptr<ExprAST> parse_primary_expression(bool function_first = true);
    std::unique_ptr<CallExprAST> parse_call_expression(std::string callee);
    std::unique_ptr<FunctionSignatureAST> parse_function_signature(bool is_extern = false);
    void parse_function_definition();
    std::unique_ptr<IfStatementAST> parse_if_statement();
    std::unique_ptr<WhileStatementAST> parse_while_statement();
    int is_variable(const std::string& name);
    int is_variable(SymbolTable* symbol_table, const std::string& name);


    std::unique_ptr<Lex> lex;
    std::vector<SymbolTableLayer> scoped_symbol_table_layer;
    GlobalTable constant_table;
    GlobalTable global_table;
    FunctionTable function_table;
    ExternFunctionTable extern_function_table;
    int token = 0;

    friend class SemanticAnalyzer;
    friend class CodeGen;
};