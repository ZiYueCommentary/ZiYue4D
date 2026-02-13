#pragma once

#include <fstream>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SourceMgr.h>
#include "Token.h"

constexpr bool is_variable_type(const SymbolType type) {
    return type == SYMBOL_TYPE_INT || type == SYMBOL_TYPE_FLOAT || type == SYMBOL_TYPE_STRING ||
           type == SYMBOL_TYPE_STRUCT || type == SYMBOL_TYPE_POINTER;
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
            throw std::exception();
    }
}

constexpr std::string symbol_type_to_literal(const SymbolType type) {
    switch (type) {
        case SYMBOL_TYPE_INT: return "integer";
        case SYMBOL_TYPE_FLOAT: return "float";
        case SYMBOL_TYPE_STRING: return "string";
        case SYMBOL_TYPE_FUNCTION: return "function";
        case SYMBOL_TYPE_STRUCT: return "struct";
        case SYMBOL_TYPE_POINTER: return "pointer";
        case SYMBOL_TYPE_VOID: return "void";
    }
    return "[unknown]";
}

constexpr bool is_end_of_stmt(const int token) {
    return token == TOKEN_LINE_FEED || token == TOKEN_COLON;
}

class Lex {
public:
    /// The literal of the identifier. All in lower case. Only valid when token is TOKEN_IDENTIFIER.
    std::string identifier;
    /// The literal of the identifier, case-sensitive.
    /// Being used when declaring an extern function.
    /// Only valid when token is TOKEN_IDENTIFIER.
    std::string case_identifier;
    /// The value of literal integer. Only valid when token is TOKEN_INTEGER.
    int int_value = 0;
    /// The value of literal string. Only valid when token is TOKEN_STRING.
    std::string string_value;
    /// The value of literal float. Only valid when token is TOKEN_FLOAT.
    float float_value = .0f;
    /// The range of literal or identifier.
    /// Note that this field is valid only when a token is a token, instead of a character.
    llvm::SMRange range;
    /// The position in the file of the token.
    /// Under most circumstances, AST has to construct llvm::Range with it by itself.
    size_t pos = 0;
    /// The position of last token. If the token takes more than one character, this points to the last one.
    size_t last_token_pos;

    llvm::SourceMgr source_mgr;

    explicit Lex(const std::string& file) {
        auto ptr = llvm::MemoryBuffer::getFile(file);
        if (ptr.get() == nullptr) throw std::exception();

        source_id = source_mgr.AddNewSourceBuffer(std::move(ptr.get()), llvm::SMLoc());
        this->file = source_mgr.getMemoryBuffer(source_id);
    }

    /// Getting a token and moving pos pointer to next.
    int get_token();
    [[nodiscard]] llvm::SMRange quick_build_range(size_t begin, size_t end) const;
    [[nodiscard]] llvm::SMLoc quick_build_loc(size_t pos) const;

private:
    const llvm::MemoryBuffer* file;
    unsigned source_id;
    int last_char = ' ';

    int next_char();
};
