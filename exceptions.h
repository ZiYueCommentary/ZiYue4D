#pragma once

#include <exception>

class ziyue4d_exception : public std::exception {
    const char* const str;

public:
    explicit ziyue4d_exception(const char* const str) : str(str) {
    }

    [[nodiscard]] const char* what() const noexcept override {
        return str;
    }
};

class lex_exception : public ziyue4d_exception {
public:
    lex_exception(char const* const str) : ziyue4d_exception(str) {
    }
};

class ast_exception : public ziyue4d_exception {
public:
    ast_exception(char const* const str) : ziyue4d_exception(str) {
    }
};

class semantic_exception : public ziyue4d_exception {
public:
    semantic_exception(char const* const str) : ziyue4d_exception(str) {
    }
};

class codegen_exception : public ziyue4d_exception {
public:
    codegen_exception(char const* const str) : ziyue4d_exception(str) {
    }
};
