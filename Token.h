#pragma once

#include <unordered_map>
#include <string>

enum Token {
    TOKEN_EOF = INT_MIN,
    TOKEN_END_OF_STMT,
    TOKEN_IDENTIFIER,
    TOKEN_FUNCTION,
    TOKEN_EXTERN,
    TOKEN_INTEGER,
    TOKEN_FLOAT,
    TOKEN_STRING,
    TOKEN_END,
    TOKEN_RETURN,
    TOKEN_CONST,
    TOKEN_GLOBAL,
    TOKEN_LOCAL,
    TOKEN_LOGIC_NOT,
    TOKEN_BITWISE_OR,
    TOKEN_LOGIC_OR,
    TOKEN_BITWISE_AND,
    TOKEN_LOGIC_AND,
    TOKEN_NOT_EQUALS,
    TOKEN_EQUALS,
    TOKEN_LESS_THAN,
    TOKEN_LESS_THAN_OR_EQUALS,
    TOKEN_GREATER_THAN,
    TOKEN_GREATER_THAN_OR_EQUALS,
    TOKEN_TYPE_INT,
    TOKEN_TYPE_FLOAT,
    TOKEN_TYPE_STRING,
    TOKEN_TYPE_POINTER
};

enum SymbolType {
    SYMBOL_TYPE_INT = -10,
    SYMBOL_TYPE_FLOAT,
    SYMBOL_TYPE_STRING,
    SYMBOL_TYPE_FUNCTION,
    SYMBOL_TYPE_STRUCT,
    SYMBOL_TYPE_VOID,
    SYMBOL_TYPE_POINTER
};

const std::unordered_map<std::string, Token> tokens = {
    {"function", TOKEN_FUNCTION},
    {"not", TOKEN_LOGIC_NOT},
    {"end", TOKEN_END},
    {"extern", TOKEN_EXTERN},
    {"return", TOKEN_RETURN},
    {"const", TOKEN_CONST},
    {"local", TOKEN_LOCAL},
    {"global", TOKEN_GLOBAL},
    {"and", TOKEN_BITWISE_AND},
    {"or", TOKEN_BITWISE_OR},
    {"lor", TOKEN_LOGIC_OR},
};