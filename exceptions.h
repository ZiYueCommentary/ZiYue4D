#pragma once

#include <exception>

class lex_exception : public std::exception {
public:
    lex_exception(char const* const str) : exception(str) {}
};

class ast_exception : public std::exception {
public:
    ast_exception(char const* const str) : exception(str) {}
};

class semantic_exception : public std::exception {
public:
    semantic_exception(char const* const str) : std::exception(str) {}
};