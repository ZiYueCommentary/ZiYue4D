#include "AST.h"
#include <algorithm>
#include <iostream>

const std::unordered_map<int, int> op_precedence = {
    {'=', 10},
    {TOKEN_NOT_EQUALS, 20},
    {'+', 30}, {'-', 30}, {'*', 31}, {'/', 31}
};

bool AST::parse() {
    bool occur_errors = false;
    global_symbols.insert({"main", SYMBOL_TYPE_FUNCTION});
    auto signature = std::make_unique<FunctionSignatureAST>("main", SYMBOL_TYPE_INT);
    auto function = std::make_unique<FunctionAST>(std::move(signature));
    function_table.emplace("main", std::move(function));
    const auto &main = function_table.equal_range("main").first->second;
    while (true) {
        try {
            this->token = lex->get_token();
            if (token == TOKEN_EOF) break;
            if (token == TOKEN_END_OF_STMT) continue;
            if (token == TOKEN_FUNCTION) {
                parse_function_definition();
                continue;
            }
            if (token == TOKEN_EXTERN) {
                parse_function_signature(true);
                continue;
            }
            std::unique_ptr<ExprAST> lhs = std::move(parse_primary_expression(main->signature->symbol_table));
            main->body.push_back(std::move(parse_expression(std::move(lhs), main->signature->symbol_table)));
        } catch (ast_exception &e) {
            std::cerr << e.what() << " at " << lex->line << ':' << lex->pos << '\n';
            while (token != TOKEN_EOF && token != TOKEN_END_OF_STMT) { this->token = lex->get_token(); }
            occur_errors = true;
        }
        catch (lex_exception &e) {
            std::cerr << e.what() << " at " << lex->line << ':' << lex->pos << '\n';
            while (token != TOKEN_EOF && token != TOKEN_END_OF_STMT) { this->token = lex->get_token(); }
            occur_errors = true;
        }
    }

    return occur_errors;
}

std::unique_ptr<ExprAST> AST::parse_primary_expression(SymbolTable &symbol_table, bool function_first) {
    std::unique_ptr<ExprAST> lhs = nullptr;
    switch (token) {
        case TOKEN_GLOBAL:
            do {
                token = lex->get_token();
                if (is_variable(global_symbols, lex->identifier)) throw ast_exception("duplicate variable definition");
                lhs = parse_expression(parse_primary_expression(global_symbols, function_first), global_symbols,
                                       function_first);
            } while (token == ',');
            break;
        case TOKEN_LOCAL:
            do {
                token = lex->get_token();
                if (is_variable(symbol_table, lex->identifier)) throw ast_exception("duplicate variable definition");
                lhs = parse_expression(parse_primary_expression(symbol_table, function_first), symbol_table,
                                       function_first);
            } while (token == ',');
            break;
        case TOKEN_CONST: {
            if (&symbol_table != &function_table.equal_range("main").first->second->signature->symbol_table) {
                throw ast_exception("constant cannot be defined in function");
            }

            do {
                token = lex->get_token();
                std::string identifier = std::move(lex->identifier);
                if (is_variable(global_symbols, identifier)) throw ast_exception("duplicate constant definition");
                Token type = TOKEN_TYPE_INT;
                token = lex->get_token();
                switch (token) {
                    case TOKEN_TYPE_FLOAT:
                    case TOKEN_TYPE_STRING:
                    case TOKEN_TYPE_POINTER:
                    case TOKEN_TYPE_INT:
                        type = static_cast<Token>(token);
                        token = lex->get_token();
                }

                if (token != '=') throw ast_exception("missing constant value");
                token = lex->get_token();
                global_symbols.insert({identifier, token_to_type(type)});
                constant_table.emplace(identifier, std::move(
                                           parse_expression(parse_primary_expression(global_symbols, false),
                                                            global_symbols, false)));
                lhs = std::make_unique<VariableExprAST>(std::move(identifier));
            } while (token == ',');
            break;
        }
        case TOKEN_IDENTIFIER: {
            std::string identifier = std::move(lex->identifier);
            Token type = TOKEN_TYPE_INT;
            token = lex->get_token();
            switch (token) {
                case TOKEN_TYPE_FLOAT:
                    if (!is_variable(symbol_table, identifier)) {
                        symbol_table.insert({identifier, SYMBOL_TYPE_FLOAT});
                    }
                    token = lex->get_token();
                    break;
                case TOKEN_TYPE_STRING:
                    if (!is_variable(symbol_table, identifier)) {
                        symbol_table.insert({identifier, SYMBOL_TYPE_STRING});
                    }
                    token = lex->get_token();
                    break;
                case TOKEN_TYPE_POINTER:
                    if (!is_variable(symbol_table, identifier)) {
                        symbol_table.insert({identifier, SYMBOL_TYPE_POINTER});
                    }
                    token = lex->get_token();
                    break;
                case TOKEN_TYPE_INT:
                    if (!is_variable(symbol_table, identifier)) {
                        symbol_table.insert({identifier, SYMBOL_TYPE_INT});
                    }
                    token = lex->get_token();
                default:
                    if (token == '=') {
                        if (const int symbol_type = is_variable(symbol_table, identifier); symbol_type == 0) {
                            symbol_table.insert({identifier, SYMBOL_TYPE_INT});
                        } else if (symbol_type != token_to_type(type)) {
                            throw ast_exception("mismatched variable type");
                        }
                        break;
                    }
                    if (token == '(' || function_first) {
                        // must be function call
                        lhs = parse_call_expression(std::move(identifier), symbol_table);
                        return lhs;
                    }
            }

            lhs = std::make_unique<VariableExprAST>(std::move(identifier));
            break;
        }
        case TOKEN_INTEGER:
            lhs = std::make_unique<IntegerExprAST>(lex->int_value);
            token = lex->get_token();
            break;
        case TOKEN_FLOAT:
            lhs = std::make_unique<FloatExprAST>(lex->float_value);
            token = lex->get_token();
            break;
        case TOKEN_STRING:
            lhs = std::make_unique<StringExprAST>(std::move(lex->string_value));
            token = lex->get_token();
            break;
        case '(':
            token = lex->get_token();
            lhs = std::move(parse_expression(std::move(parse_primary_expression(symbol_table, function_first)),
                                             symbol_table, function_first));
            if (token != ')') throw ast_exception("expecting closing parenthesis");
            token = lex->get_token();
            break;
        case '&':
            token = lex->get_token();
            if (token == TOKEN_IDENTIFIER) {
                lhs = std::make_unique<UnaryExprAST>(
                    '&', std::move(std::make_unique<VariableExprAST>(std::move(lex->identifier))));
                token = lex->get_token();
            } else {
                throw ast_exception("expecting identifier");
            }
            break;
        case '-':
            token = lex->get_token();
            lhs = std::make_unique<
                UnaryExprAST>('-', std::move(parse_primary_expression(symbol_table, function_first)));
            break;
        case TOKEN_LOGIC_NOT:
            token = lex->get_token();
            lhs = std::make_unique<UnaryExprAST>(TOKEN_LOGIC_NOT,
                                                 std::move(parse_primary_expression(symbol_table, function_first)));
            break;
        case TOKEN_RETURN:
            token = lex->get_token();
            if (token == TOKEN_EOF || token == TOKEN_END_OF_STMT) {
                lhs = std::make_unique<ReturnExprAST>(nullptr);
                break;
            }
            lhs = std::make_unique<ReturnExprAST>(std::move(
                parse_expression(std::move(parse_primary_expression(symbol_table, false)), symbol_table, false)));
            break;
        case TOKEN_IF:
            lhs = parse_if_statement(symbol_table);
            break;
        default:
            throw ast_exception("expecting primary expression");
    }
    return lhs;
}

std::unique_ptr<FunctionSignatureAST> AST::parse_function_signature(bool is_extern) {
    this->token = lex->get_token();
    if (token != TOKEN_IDENTIFIER) throw ast_exception("expecting function name");
    std::string name = std::move(is_extern ? lex->case_identifier : lex->identifier);
    std::string non_case_name = is_extern ? std::move(lex->identifier) : name;
    this->token = lex->get_token();
    SymbolType return_value_type = is_extern ? SYMBOL_TYPE_VOID : SYMBOL_TYPE_INT;
    if (token == TOKEN_TYPE_INT || token == TOKEN_TYPE_FLOAT || token == TOKEN_TYPE_STRING || token ==
        TOKEN_TYPE_POINTER) {
        return_value_type = token_to_type(static_cast<Token>(token));
        this->token = lex->get_token();
    }
    if (token != '(') throw ast_exception("expecting opening parenthesis");
    auto function = std::make_unique<FunctionSignatureAST>(name, return_value_type);
    int mandatory_args = 0, optional_args = 0;
    do {
        this->token = lex->get_token();
        if (token == ')') break;
        if (token != TOKEN_IDENTIFIER) throw ast_exception("expecting argument name");
        std::string arg_name = std::move(lex->identifier);
        this->token = lex->get_token();
        SymbolType type = SYMBOL_TYPE_INT;
        if (token == TOKEN_TYPE_INT || token == TOKEN_TYPE_FLOAT || token == TOKEN_TYPE_STRING || token ==
            TOKEN_TYPE_POINTER) {
            type = token_to_type(static_cast<Token>(token));
            this->token = lex->get_token();
        }
        std::unique_ptr<ExprAST> default_value = nullptr;
        if (token == '=') {
            this->token = lex->get_token();
            default_value = parse_expression(parse_primary_expression(function->symbol_table, false),
                                             function->symbol_table, false);
            optional_args++;
        } else {
            mandatory_args++;
        }
        function->symbol_table.insert({arg_name, type});
        function->arguments.push_back(
            std::make_unique<FunctionArgument>(std::move(arg_name), type, std::move(default_value)));
    } while (token == ',');
    if (token != ')') throw ast_exception("expecting closing parenthesis");
    global_symbols.insert({non_case_name, SYMBOL_TYPE_FUNCTION});
    if (is_extern) {
        if (extern_function_table.contains(name)) throw ast_exception("duplicate extern function");
        extern_function_table.emplace(non_case_name, std::move(function));
        return nullptr;
    }

    // looking for duplicate signatures...
    auto defined = function_table.equal_range(function->name);
    for (auto &it = defined.first; it != defined.second; ++it) {
        const size_t define_mandatory_args = std::ranges::count_if(it->second->signature->arguments,
                                                                   [](const std::unique_ptr<FunctionArgument> &arg) {
                                                                       return arg->default_value == nullptr;
                                                                   });
        const size_t define_optional_args = it->second->signature->arguments.size() - define_mandatory_args;
        if (define_mandatory_args == mandatory_args && define_optional_args == optional_args) {
            throw ast_exception("duplicate function signature");
        }
    }

    return function;
}

void AST::parse_function_definition() {
    auto function = std::make_unique<FunctionAST>(std::move(parse_function_signature()));
    this->token = lex->get_token();
    do {
        if (token == TOKEN_EOF) throw ast_exception("expecting end function");
        if (token == TOKEN_FUNCTION) throw ast_exception("cannot define function in function");
        if (token == TOKEN_EXTERN) throw ast_exception("cannot define extern function in function");
        if (token == TOKEN_CONST) throw ast_exception("cannot define constant in function");
        if (token == TOKEN_GLOBAL) throw ast_exception("cannot define global in function");
        if (token == TOKEN_END && (this->token = lex->get_token()) == TOKEN_FUNCTION) { break; }
        if (token == TOKEN_END_OF_STMT) {
            this->token = lex->get_token();
            continue;
        }
        std::unique_ptr<ExprAST> lhs = std::move(parse_primary_expression(function->signature->symbol_table));
        function->body.push_back(std::move(parse_expression(std::move(lhs), function->signature->symbol_table)));
    } while (true);
    function_table.emplace(function->signature->name, std::move(function));
}

std::unique_ptr<IfStatementAST> AST::parse_if_statement(SymbolTable &symbol_table) {
    this->token = lex->get_token();
    std::unique_ptr<ExprAST> condition = parse_expression(parse_primary_expression(symbol_table, false), symbol_table,
                                                          false);
    if (this->token == TOKEN_THEN) this->token = lex->get_token();
    std::unique_ptr<IfStatementAST> statement = std::make_unique<IfStatementAST>(std::move(condition));
    if (this->token != TOKEN_END_OF_STMT) {
        statement->statement_true.push_back(parse_expression(
            parse_primary_expression(statement->statement_true_symbol_table), statement->statement_true_symbol_table));
        if (this->token == TOKEN_ELSE) {
            this->token = lex->get_token();
            statement->statement_false.push_back(parse_expression(
                parse_primary_expression(statement->statement_false_symbol_table),
                statement->statement_false_symbol_table));
        } else if (this->token == TOKEN_ELSE_IF) {
            statement->statement_false.push_back(parse_if_statement(statement->statement_false_symbol_table));
        }
        return statement;
    }
    do {
        if (token == TOKEN_EOF) throw ast_exception("expecting endif");
        if (token == TOKEN_FUNCTION) throw ast_exception("cannot define function in if statement");
        if (token == TOKEN_EXTERN) throw ast_exception("cannot define extern function in if statement");
        if (token == TOKEN_CONST) throw ast_exception("cannot define constant in if statement");
        if (token == TOKEN_GLOBAL) throw ast_exception("cannot define global in if statement");
        if (token == TOKEN_END && (this->token = lex->get_token()) == TOKEN_IF) { break; }
        if (token == TOKEN_END_OF_STMT) {
            this->token = lex->get_token();
            continue;
        }
        statement->statement_true.push_back(parse_expression(
            parse_primary_expression(statement->statement_true_symbol_table), statement->statement_true_symbol_table));
    } while (this->token != TOKEN_ELSE && this->token != TOKEN_ELSE_IF && this->token != TOKEN_ENDIF);
    if (this->token == TOKEN_ELSE_IF) {
        statement->statement_false.push_back(parse_if_statement(statement->statement_false_symbol_table));
    } else {
        if (this->token == TOKEN_ELSE) {
            this->token = lex->get_token();
            do {
                if (token == TOKEN_EOF) throw ast_exception("expecting endif");
                if (token == TOKEN_FUNCTION) throw ast_exception("cannot define function in if statement");
                if (token == TOKEN_EXTERN) throw ast_exception("cannot define extern function in if statement");
                if (token == TOKEN_CONST) throw ast_exception("cannot define constant in if statement");
                if (token == TOKEN_GLOBAL) throw ast_exception("cannot define global in if statement");
                if (token == TOKEN_END && (this->token = lex->get_token()) == TOKEN_IF) { break; }
                if (token == TOKEN_END_OF_STMT) {
                    this->token = lex->get_token();
                    continue;
                }
                statement->statement_false.push_back(parse_expression(
                    parse_primary_expression(statement->statement_false_symbol_table),
                    statement->statement_false_symbol_table));
            } while (this->token != TOKEN_ELSE && this->token != TOKEN_ELSE_IF && this->token != TOKEN_ENDIF);
        }

        if (this->token != TOKEN_ENDIF) {
            if (this->token != TOKEN_END || ((this->token = lex->get_token()) != TOKEN_IF))
                throw ast_exception(
                    "expecting endif");
        }
        this->token = lex->get_token();
    }
    return statement;
}

std::unique_ptr<CallExprAST> AST::parse_call_expression(std::string callee, SymbolTable &symbol_table) {
    std::vector<std::unique_ptr<ExprAST> > arguments = {};
    if (token == ')') return std::make_unique<CallExprAST>(std::move(callee), std::move(arguments));
    do {
        if (token == ',' || token == '(') this->token = lex->get_token();
        if (token == ')' || token == TOKEN_EOF || token == TOKEN_END_OF_STMT) {
            this->token = lex->get_token();
            break;
        }
        std::unique_ptr<ExprAST> lhs = std::move(parse_primary_expression(symbol_table, false));
        arguments.push_back(std::move(parse_expression(std::move(lhs), symbol_table, false)));
    } while (token == ',');
    if (token == ')') this->token = lex->get_token();
    return std::make_unique<CallExprAST>(std::move(callee), std::move(arguments));
}

std::unique_ptr<ExprAST> AST::parse_expression(std::unique_ptr<ExprAST> lhs, SymbolTable &symbol_table,
                                               bool function_first) {
    while (true) {
        int op = token;
        if (token == TOKEN_EOF || token == TOKEN_END_OF_STMT || token == TOKEN_THEN || token == TOKEN_ELSE || token ==
            TOKEN_ELSE_IF || token == ')' || token == ',')
            return lhs;

        token = lex->get_token();
        std::unique_ptr<ExprAST> rhs = std::move(
            parse_primary_expression(symbol_table, op == '=' ? false : function_first));

        while (token != TOKEN_EOF && token != TOKEN_END_OF_STMT && token != TOKEN_THEN && token != TOKEN_ELSE && token
               != TOKEN_ELSE_IF && token != ')' && token != ',') {
            if (op_precedence.at(op) < op_precedence.at(token)) {
                int next_op = token;
                token = lex->get_token();
                rhs = std::make_unique<BinaryExprAST>(next_op, std::move(rhs),
                                                      parse_expression(
                                                          std::move(parse_primary_expression(
                                                              symbol_table, op == '=' ? false : function_first)),
                                                          symbol_table, op == '=' ? false : function_first));
            }
        }

        lhs = std::make_unique<BinaryExprAST>(op == '=' ? (function_first ? '=' : TOKEN_EQUALS) : op, std::move(lhs),
                                              std::move(rhs));
    }
}

int AST::is_variable(const SymbolTable &symbol_table, const std::string &name) {
    if (symbol_table.contains(name)) {
        auto range = symbol_table.equal_range(name);
        for (auto it = range.first; it != range.second; ++it) {
            if (is_variable_type(it->second)) return it->second;
        }
    }
    if (!global_symbols.contains(name)) return 0;
    auto range = global_symbols.equal_range(name);
    for (auto it = range.first; it != range.second; ++it) {
        if (is_variable_type(it->second)) return it->second;
    }
    return 0;
}
