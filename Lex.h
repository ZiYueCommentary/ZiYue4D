#pragma once

#include <fstream>
#include <memory>
#include "exceptions.h"
#include "Token.h"

constexpr bool is_variable_type(const SymbolType type) {
    return type == SYMBOL_TYPE_INT || type == SYMBOL_TYPE_FLOAT || type == SYMBOL_TYPE_STRING || type ==
           SYMBOL_TYPE_STRUCT || type == SYMBOL_TYPE_POINTER;
}

constexpr SymbolType token_to_type(const Token token) {
    switch (token) {
        case TOKEN_TYPE_INT:
            return SYMBOL_TYPE_INT;
        case TOKEN_TYPE_FLOAT:
            return SYMBOL_TYPE_FLOAT;
        case TOKEN_TYPE_STRING:
            return SYMBOL_TYPE_STRING;
        case TOKEN_TYPE_POINTER:
            return SYMBOL_TYPE_POINTER;
        default:
            throw lex_exception("invalid type token");
    }
}

constexpr bool is_end_of_stmt(const int token) {
    return token == TOKEN_LINE_FEED || token == TOKEN_COLON;
}

class Lex {
public:
    std::string identifier;
    std::string case_identifier;
    int int_value = 0;
    std::string string_value;
    float float_value = .0f;
    size_t line = 1, pos = 0;

    explicit Lex(const std::string& file) {
        this->file = std::move(std::make_unique<std::ifstream>(file));

        if (!this->file->good()) throw lex_exception("Failed to open source file");
    }

    int get_token();

private:
    std::unique_ptr<std::ifstream> file;
    int last_char = ' ';

    int next_char();
};
