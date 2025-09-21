#include "Lex.h"

#include <string>
#include <unordered_set>
#include <iostream>

const std::unordered_set<char> operator_chars = {
    '+', '-', '*', '/', '%', '=', '<', '>', '!', '&', '|',
    '^', '~', '?', ':', '.', ',', ';', '(', ')', '[', ']', '{', '}', '$', '#',
    EOF, '\n', '\r', ' '
};

bool is_alpha(int c) {
    return ((c & 0x80) != 0 || !operator_chars.contains(c)) && c != EOF;
}

int Lex::get_token() {
    static int last_char = ' ';    

    if (last_char == '\n' || last_char == ':') { last_char = file->get(); return TOKEN_END_OF_STMT; }

    while (isspace(last_char)) last_char = file->get();
    if (last_char == '%') { last_char = file->get(); return TOKEN_TYPE_INT; }
    if (last_char == '#') { last_char = file->get(); return TOKEN_TYPE_FLOAT; }
    if (last_char == '$') { last_char = file->get(); return TOKEN_TYPE_STRING; }
    if (last_char == '!') { last_char = file->get(); return TOKEN_LOGIC_NOT; }
    if (last_char == '*') { last_char = file->get(); return TOKEN_TYPE_POINTER; }

    if (last_char == '\"') {
        string_value.clear();
        while ((last_char = file->get()) != '\"') {
            if (last_char == '\n') throw lex_exception("mismatched quotes");
            string_value += last_char;
        }
        last_char = file->get();
        return TOKEN_STRING;
    }

    if (isdigit(last_char) || last_char == '.') {
        std::string number;
        Token type = TOKEN_INTEGER;
        do {
            if (last_char != '_') {
                number += last_char;
                if (last_char == '.') type = TOKEN_FLOAT;
            }
            last_char = file->get();
        } while (isdigit(last_char) || last_char == '.' || last_char == '_');
        if (type == TOKEN_INTEGER) {
            int_value = atoi(number.c_str());
        }
        else {
            float_value = strtof(number.c_str(), nullptr);
        }
        return type;
    }

    if (is_alpha(last_char)) {
        identifier = tolower(last_char);
        while (is_alpha(last_char = file->get()) || isdigit(last_char)) identifier += tolower(last_char);
        if (tokens.contains(identifier)) return tokens.at(identifier);
        return TOKEN_IDENTIFIER;
    }

    if (last_char == ';') {
        do {
            last_char = file->get();
        } while (last_char != EOF && last_char != '\n' && last_char != '\r');
        if (last_char != EOF) return get_token();
    }

    if (last_char == EOF) return TOKEN_EOF;

    int curr_char = last_char;
    last_char = file->get();
    return curr_char;
}