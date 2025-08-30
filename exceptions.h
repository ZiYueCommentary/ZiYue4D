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