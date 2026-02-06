#include "Lex.h"

#include <iostream>
#include <string>
#include <unordered_set>

const std::unordered_set<char> operator_chars = {
    '+', '-', '*', '/', '%', '=', '<', '>', '!', '&', '|',
    '^', '~', '?', ':', '.', ',', ';', '(', ')', '[', ']', '{', '}', '$', '#', '@',
    EOF, '\n', '\r', ' '
};

std::string parse_string_literal(const std::string& raw) {
    std::string result;
    for (size_t i = 0; i < raw.length(); ++i) {
        if (raw[i] == '\\' && i + 1 < raw.length()) {
            switch (char next = raw[++i]) {
                case 'n': result += '\n';
                    break;
                case 't': result += '\t';
                    break;
                case 'r': result += '\r';
                    break;
                case '\\': result += '\\';
                    break;
                case '"': result += '"';
                    break;
                case '\'': result += '\'';
                    break;
                case 'b': result += '\b';
                    break;
                case 'f': result += '\f';
                    break;
                case 'x': {
                    if (i + 2 < raw.length()) {
                        std::string hex = raw.substr(i + 1, 2);
                        result += static_cast<char>(std::stoi(hex, nullptr, 16));
                        i += 2;
                    }
                    break;
                }
                default: result += next;
                    break;
            }
        } else {
            result += raw[i];
        }
    }
    return result;
}

bool is_alpha(int c) {
    return ((c & 0x80) != 0 || !operator_chars.contains(c)) && c != EOF;
}

int Lex::next_char() {
    last_char = file->get();
    if (last_char == '\n') {
        line++;
        pos = 0;
    } else {
        pos++;
    }
    return last_char;
}

int Lex::get_token() {
    while (isspace(last_char) && last_char != '\r' && last_char != '\n') next_char();
    if (last_char == '\n' || last_char == '\r') {
        next_char();
        return TOKEN_LINE_FEED;
    }
    if (last_char == ':') {
        next_char();
        return TOKEN_COLON;
    }

    if (last_char == ';') {
        do {
            next_char();
        } while (last_char != EOF && last_char != '\n' && last_char != '\r');
        if (last_char != EOF) return get_token();
    }

    if (last_char == '/') {
        if (next_char() == '*') {
            do {
                if (last_char == '*') {
                    if ((next_char()) == '/') break;
                }
                next_char();
            } while (last_char != EOF);
            if (last_char != EOF) next_char();
            if (last_char != EOF) return get_token();
        } else {
            return '/';
        }
    }

    if (last_char == '%') {
        if (next_char() == '0' || last_char == '1') {
            std::string bin_int = std::string(1, static_cast<char>(last_char));
            while (next_char() == '0' || last_char == '1') {
                bin_int += static_cast<char>(last_char);
            }
            int_value = std::stoi(bin_int, nullptr, 2);
            return TOKEN_INTEGER;
        }
        return TOKEN_TYPE_INT;
    }
    if (last_char == '$') {
        if (isxdigit(next_char())) {
            std::string hex_int = std::string(1, static_cast<char>(last_char));
            while (isxdigit(next_char())) {
                hex_int += static_cast<char>(last_char);
            }
            int_value = std::stoi(hex_int, nullptr, 16);
            return TOKEN_INTEGER;
        }
        return TOKEN_TYPE_STRING;
    }
    if (last_char == '#') {
        next_char();
        return TOKEN_TYPE_FLOAT;
    }
    if (last_char == '@') {
        next_char();
        return TOKEN_TYPE_POINTER;
    }

    if (last_char == '!') {
        next_char();
        if (last_char == '=') {
            next_char();
            return TOKEN_NOT_EQUALS;
        }
        return TOKEN_LOGIC_NOT;
    }
    if (last_char == '<') {
        next_char();
        switch (last_char) {
            case '>':
                next_char();
                return TOKEN_NOT_EQUALS;
            case '=':
                next_char();
                return TOKEN_LESS_THAN_OR_EQUALS;
            default:
                return TOKEN_LESS_THAN;
        }
    }
    if (last_char == '>') {
        next_char();
        switch (last_char) {
            case '=':
                next_char();
                return TOKEN_GREATER_THAN_OR_EQUALS;
            default:
                return TOKEN_GREATER_THAN;
        }
    }
    if (last_char == '=') {
        next_char();
        if (last_char == '=') {
            next_char();
            return TOKEN_EQUALS;
        }
        return '=';
    }
    if (last_char == '&') {
        next_char();
        if (last_char == '&') {
            next_char();
            return TOKEN_LOGIC_AND;
        }
        return TOKEN_BITWISE_AND;
    }
    if (last_char == '|') {
        next_char();
        if (last_char == '|') {
            next_char();
            return TOKEN_LOGIC_OR;
        }
        return TOKEN_BITWISE_OR;
    }

    if (last_char == '\"') {
        string_value.clear();
        while ((next_char()) != '\"') {
            if (last_char == '\n') throw lex_exception("mismatched quotes");
            string_value += last_char;
            if (last_char == '\\') {
                string_value += (next_char());
            }
        }
        next_char();
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
            next_char();
        } while (isdigit(last_char) || last_char == '.' || last_char == '_');
        if (type == TOKEN_INTEGER) {
            int_value = std::atoi(number.c_str());
        } else {
            float_value = std::strtof(number.c_str(), nullptr);
        }
        return type;
    }

    if (is_alpha(last_char)) {
        identifier = tolower(last_char);
        case_identifier = last_char;
        while (is_alpha(next_char()) || isdigit(last_char)) {
            identifier += tolower(last_char);
            case_identifier += last_char;
        }
        if (tokens.contains(identifier)) return tokens.at(identifier);
        return TOKEN_IDENTIFIER;
    }

    if (last_char == EOF) return TOKEN_EOF;

    const int curr_char = last_char;
    next_char();
    return curr_char;
}
