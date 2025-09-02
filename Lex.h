#pragma once

#include "Token.h"
#include "exceptions.h"
#include <fstream>

constexpr bool is_variable_type(SymbolType type) {
    return type == SYMBOL_TYPE_INT || type == SYMBOL_TYPE_FLOAT || type == SYMBOL_TYPE_STRING || type == SYMBOL_TYPE_STRUCT;
}

constexpr SymbolType token_to_type(Token token) {
    switch (token)
    {
    case TOKEN_TYPE_INT:
        return SYMBOL_TYPE_INT;
    case TOKEN_TYPE_FLOAT:
        return SYMBOL_TYPE_FLOAT;
    case TOKEN_TYPE_STRING:
        return SYMBOL_TYPE_STRING;
    default:
        throw lex_exception("invalid type token");
    }
}

class Lex {
public:
    std::string identifier = "";
    int int_value = 0;
    std::string string_value = "";
    float float_value = .0f;

    Lex(std::string file) {
        this->file = std::move(std::make_unique<std::ifstream>(file));

        if (!this->file->good()) throw std::exception("Failed to open source file");
    }

    int get_token();

private:
    std::unique_ptr<std::ifstream> file;
};