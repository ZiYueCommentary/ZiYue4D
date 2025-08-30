#include "Lex.h"

int Lex::get_token() {
    static int last_char = ' ';    

    if (last_char == '\n' || last_char == ':') { last_char = file->get(); return TOKEN_END_OF_STMT; }

    while (isspace(last_char)) last_char = file->get();
    if (last_char == '%') { last_char = file->get(); return TOKEN_TYPE_INT; }
    if (last_char == '#') { last_char = file->get(); return TOKEN_TYPE_FLOAT; }
    if (last_char == '$') { last_char = file->get(); return TOKEN_TYPE_STRING; }
    if (last_char == '!') { last_char = file->get(); return TOKEN_LOGIC_NOT; }

    if (last_char == '\"') {
        string_value.clear();
        while ((last_char = file->get()) != '\"') {
            if (last_char == '\n') throw lex_exception("mismatched quotes");
            string_value += last_char;
        }
        last_char = file->get();
        return TOKEN_STRING;
    }

    if (isalpha(last_char)) {
        identifier = tolower(last_char);
        while (isalnum(last_char = file->get())) identifier += tolower(last_char);
        if (tokens.contains(identifier)) return tokens.at(identifier);
        return TOKEN_IDENTIFIER;
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