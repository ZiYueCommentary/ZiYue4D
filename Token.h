#pragma once

#include <unordered_map>
#include <string>

enum Token {
    TOKEN_EOF = -20,
    TOKEN_END_OF_STMT,
    TOKEN_IDENTIFIER,
    TOKEN_FUNCTION,
    TOKEN_INTEGER,
    TOKEN_FLOAT,
    TOKEN_STRING,
    TOKEN_END,
    TOKEN_LOGIC_NOT,
    TOKEN_TYPE_INT,
    TOKEN_TYPE_FLOAT,
    TOKEN_TYPE_STRING
};

const std::unordered_map<std::string, Token> tokens = {
    {"function", TOKEN_FUNCTION},
    {"not", TOKEN_LOGIC_NOT},
    {"end", TOKEN_END}
};