#include "Lex.h"

#include <string>
#include <unordered_set>
#include <iostream>

const std::unordered_set<char> operator_chars = {
    '+', '-', '*', '/', '%', '=', '<', '>', '!', '&', '|',
    '^', '~', '?', ':', '.', ',', ';', '(', ')', '[', ']', '{', '}', '$', '#', '@',
    EOF, '\n', '\r', ' '
};

std::string parse_string_literal(const std::string& raw) {
    std::string result;
    for (size_t i = 0; i < raw.length(); ++i) {
        if (raw[i] == '\\' && i + 1 < raw.length()) {
            char next = raw[++i];
            switch (next) {
            case 'n': result += '\n'; break;
            case 't': result += '\t'; break;
            case 'r': result += '\r'; break;
            case '\\': result += '\\'; break;
            case '"': result += '"'; break;
            case '\'': result += '\''; break;
            case 'b': result += '\b'; break;
            case 'f': result += '\f'; break;
            case 'x': {
                if (i + 2 < raw.length()) {
                    std::string hex = raw.substr(i + 1, 2);
                    result += static_cast<char>(std::stoi(hex, nullptr, 16));
                    i += 2;
                }
                break;
            }
                    //case 'u': {
                    //    if (i + 4 < raw.length()) {
                    //        std::string hex = raw.substr(i + 1, 4);
                    //        uint32_t codepoint = std::stoi(hex, nullptr, 16);
                    //        result += encodeUTF8(codepoint);
                    //        i += 4;
                    //    }
                    //    break;
                    //}
            default: result += next; break;
            }
        }
        else {
            result += raw[i];
        }
    }
    return result;
}

bool is_alpha(int c) {
    return ((c & 0x80) != 0 || !operator_chars.contains(c)) && c != EOF;
}

int Lex::get_token() {
    if (last_char == '\n' || last_char == ':') { last_char = file->get(); return TOKEN_END_OF_STMT; }
    while (isspace(last_char)) last_char = file->get();

    if (last_char == ';') {
        do {
            last_char = file->get();
        } while (last_char != EOF && last_char != '\n' && last_char != '\r');
        if (last_char != EOF) return get_token();
    }

    if (last_char == '/') {
        if ((last_char = file->get()) == '*') {
            do {
                if (last_char == '*') {
                    if ((last_char = file->get()) == '/') break;
                }
                last_char = file->get();
            } while (last_char != EOF);
            if (last_char != EOF) last_char = file->get();
            if (last_char != EOF) return get_token();
        }
        else {
            return '/';
        }
    }

    if (last_char == '%') {
        if ((last_char = file->get()) == '0' || last_char == '1') {
            std::string bin_int = std::string(1, (char)last_char);
            while ((last_char = file->get()) == '0' || last_char == '1') {
                bin_int += (char)last_char;
            }
            int_value = std::stoi(bin_int.c_str(), nullptr, 2);
            return TOKEN_INTEGER;
        }
        return TOKEN_TYPE_INT;
    }
    if (last_char == '$') {
        if (isxdigit(last_char = file->get())) {
            std::string hex_int = std::string(1, (char)last_char);
            while (isxdigit(last_char = file->get())) {
                hex_int += (char)last_char;
            }
            int_value = std::stoi(hex_int.c_str(), nullptr, 16);
            return TOKEN_INTEGER;
        }
        return TOKEN_TYPE_STRING;
    }
    if (last_char == '#') { last_char = file->get(); return TOKEN_TYPE_FLOAT; }
    if (last_char == '!') { last_char = file->get(); return TOKEN_LOGIC_NOT; }
    if (last_char == '@') { last_char = file->get(); return TOKEN_TYPE_POINTER; }

    if (last_char == '\"') {
        string_value.clear();
        while ((last_char = file->get()) != '\"') {
            if (last_char == '\n') throw lex_exception("mismatched quotes");
            string_value += last_char;
            if (last_char == '\\') {
                string_value += (last_char = file->get());
            }
        }
        last_char = file->get();
        string_value = parse_string_literal(string_value);
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
            int_value = std::atoi(number.c_str());
        }
        else {
            float_value = std::strtof(number.c_str(), nullptr);
        }
        return type;
    }

    if (is_alpha(last_char)) {
        identifier = tolower(last_char);
        while (is_alpha(last_char = file->get()) || isdigit(last_char)) identifier += tolower(last_char);
        if (tokens.contains(identifier)) return tokens.at(identifier);
        return TOKEN_IDENTIFIER;
    }

    if (last_char == EOF) return TOKEN_EOF;

    int curr_char = last_char;
    last_char = file->get();
    return curr_char;
}