#pragma once

#include <unordered_map>
#include <string>

enum Token {
    TOKEN_EOF = -20,
    TOKEN_END_OF_STMT,
    TOKEN_IDENTIFIER,
    TOKEN_FUNCTION,
    TOKEN_EXTERN,
    TOKEN_INTEGER,
    TOKEN_FLOAT,
    TOKEN_STRING,
    TOKEN_END,
    TOKEN_LOGIC_NOT,
    TOKEN_RETURN,
    TOKEN_TYPE_INT,
    TOKEN_TYPE_FLOAT,
    TOKEN_TYPE_STRING
};

enum SymbolType {
    SYMBOL_TYPE_INT = -5,
    SYMBOL_TYPE_FLOAT,
    SYMBOL_TYPE_STRING,
    SYMBOL_TYPE_FUNCTION,
    SYMBOL_TYPE_STRUCT
};

const std::unordered_map<std::string, Token> tokens = {
    {"function", TOKEN_FUNCTION},
    {"not", TOKEN_LOGIC_NOT},
    {"end", TOKEN_END},
    {"extern", TOKEN_EXTERN},
    {"return", TOKEN_RETURN}
};