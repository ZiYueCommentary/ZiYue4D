#pragma once

#include "Lex.h"
#include "SymbolTypes.h"
#include <iostream>

using SymbolTable = std::unordered_multimap<std::string, SymbolTypes>;

class ExprAST {
public:
    virtual ~ExprAST() {}
};

struct FunctionArgument {
public:
    const std::string name;
    const Token type;
    const std::unique_ptr<ExprAST> default_value;

    FunctionArgument(std::string&& name, Token type, std::unique_ptr<ExprAST> default_value) : name(std::move(name)), type(type), default_value(std::move(default_value)) {}
    ~FunctionArgument() {}
};

class VariableExprAST : public ExprAST {
public:
    VariableExprAST(std::string&& name) : name(name) {}

private:
    std::string name;
};

class IntegerExprAST : public ExprAST {
public:
    IntegerExprAST(int value) : value(value) {

    }

private:
    int value;
};

class FloatExprAST : public ExprAST {
public:
    FloatExprAST(float value) : value(value) {

    }

private:
    float value;
};

class StringExprAST : public ExprAST {
public:
    StringExprAST(std::string&& string) : string(string) {

    }

private:
    std::string string;
};

class UnaryExprAST : public ExprAST {
public:
    UnaryExprAST(int op, std::unique_ptr<ExprAST> expr) : op(op), expr(std::move(expr)) {}

private:
    int op;
    std::unique_ptr<ExprAST> expr;
};

class BinaryExprAST : public ExprAST {
public:
    BinaryExprAST(int op, std::unique_ptr<ExprAST> lhs, std::unique_ptr<ExprAST> rhs) : op(op), lhs(std::move(lhs)), rhs(std::move(rhs)) {

    }

private:
    int op;
    std::unique_ptr<ExprAST> lhs;
    std::unique_ptr<ExprAST> rhs;
};

class FunctionAST : public ExprAST {
public:
    FunctionAST(std::string name, Token return_value_type) : name(name), return_value_type(return_value_type) {
        this->symbol_table = {};
    }

    std::string name;
    Token return_value_type;
    std::vector<std::unique_ptr<FunctionArgument>> arguments;
    std::vector<std::unique_ptr<ExprAST>> body;
    SymbolTable symbol_table;
};

using FunctionSignature = std::tuple<Token, int>; // retval & args
using FunctionTable = std::unordered_multimap<std::string, std::tuple<FunctionSignature, std::unique_ptr<FunctionAST>>>;

class AST {
public:
    AST(std::unique_ptr<Lex> lex) : lex(std::move(lex)) {}

    std::vector<std::unique_ptr<ExprAST>> parse();

private:
    std::unique_ptr<ExprAST> parse_expression(std::unique_ptr<ExprAST> lhs, SymbolTable& symbol_table);
    std::unique_ptr<ExprAST> parse_primary_expression(SymbolTable& symbol_table);
    void parse_function_definition();
    bool is_variable(SymbolTable& symbol_table, const std::string& name);
    bool is_function(const std::string& name);

    std::unique_ptr<Lex> lex;
    SymbolTable global_symbols;
    FunctionTable function_table;
    int token = 0;
};