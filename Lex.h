#pragma once

#include "Token.h"
#include "exceptions.h"
#include <fstream>

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