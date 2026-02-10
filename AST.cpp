#include "AST.h"
#include <algorithm>
#include <iostream>
#include <ranges>

#include "termcolor.hpp"

const std::unordered_map<int, int> op_precedence = {
    {'=', 10},
    {TOKEN_NOT_EQUALS, 20},
    {'+', 30}, {'-', 30}, {'*', 31}, {'/', 31}
};

bool AST::parse() {
    bool occur_errors = false;
    scoped_symbol_table_layer.emplace_back(new SymbolTable{}, SYMBOL_TABLE_TYPE_GLOBAL); // global
    scoped_symbol_table_layer.front().symbol_table->insert({"main", SYMBOL_TYPE_FUNCTION});
    auto signature = std::make_unique<FunctionSignatureAST>("main", SYMBOL_TYPE_INT);
    auto function = std::make_unique<FunctionAST>(std::move(signature));
    scoped_symbol_table_layer.emplace_back(&function->signature->symbol_table); // main local
    function_table.emplace("main", std::move(function));
    const auto& main = function_table.equal_range("main").first->second;
    while (true) {
        try {
            this->token = lex->get_token();
            if (token == TOKEN_EOF) break;
            if (is_end_of_stmt(token)) continue;
            if (token == TOKEN_FUNCTION) {
                parse_function_definition();
                continue;
            }
            if (token == TOKEN_EXTERN) {
                parse_function_signature(true);
                continue;
            }
            if (token == TOKEN_EXIT || token == TOKEN_CONTINUE)
                throw ast_exception("exit or continue can be used in a while statement only");
            std::unique_ptr<ExprAST> lhs = std::move(parse_primary_expression());
            main->body.push_back(std::move(parse_expression(std::move(lhs))));
        } catch (ast_exception& e) {
            std::cerr << termcolor::red << e.what() << " at " << lex->line << ':' << lex->pos << '\n' <<
                    termcolor::reset;
            while (token != TOKEN_EOF && !is_end_of_stmt(token)) {
                this->token = lex->get_token();
            }
            occur_errors = true;
        }
        catch (lex_exception& e) {
            std::cerr << termcolor::red << e.what() << " at " << lex->line << ':' << lex->pos << '\n' <<
                    termcolor::reset;
            while (token != TOKEN_EOF && !is_end_of_stmt(token)) {
                this->token = lex->get_token();
            }
            occur_errors = true;
        }
    }
    scoped_symbol_table_layer.pop_back(); // main local

    return occur_errors;
}

std::unique_ptr<ExprAST> AST::parse_primary_expression(bool function_first) {
    std::unique_ptr<ExprAST> lhs = nullptr;
    switch (token) {
        case TOKEN_GLOBAL:
            if (scoped_symbol_table_layer.size() > 2) {
                throw ast_exception("global cannot be defined in function");
            }
            do {
                token = lex->get_token();
                std::string identifier = std::move(lex->identifier);
                if (is_variable(scoped_symbol_table_layer.front().symbol_table, identifier)) {
                    throw ast_exception("duplicate global variable definition");
                }
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

                scoped_symbol_table_layer.front().symbol_table->insert({identifier, token_to_type(type)});
                if (token == '=') {
                    token = lex->get_token();
                    global_table.emplace(identifier,
                                         std::move(parse_expression(parse_primary_expression(false), false)));
                } else {
                    global_table.emplace(identifier, nullptr);
                }
                lhs = std::make_unique<VariableExprAST>(std::move(identifier));
            } while (token == ',');
            break;
        case TOKEN_LOCAL:
            do {
                token = lex->get_token();
                if (is_variable(lex->identifier)) throw ast_exception("duplicate variable definition");
                lhs = parse_expression(parse_primary_expression(function_first), function_first);
            } while (token == ',');
            break;
        case TOKEN_CONST: {
            if (scoped_symbol_table_layer.size() > 2) {
                throw ast_exception("constant cannot be defined in function");
            }

            do {
                token = lex->get_token();
                std::string identifier = std::move(lex->identifier);
                if (is_variable(scoped_symbol_table_layer.front().symbol_table, identifier)) {
                    throw ast_exception("duplicate constant definition");
                }
                Token type = TOKEN_TYPE_INT;
                token = lex->get_token();
                switch (token) {
                    case TOKEN_TYPE_FLOAT:
                    case TOKEN_TYPE_STRING:
                    case TOKEN_TYPE_INT:
                        type = static_cast<Token>(token);
                        token = lex->get_token();
                        break;
                    default:
                        throw ast_exception("unsupported constant type");
                }

                if (token != '=') throw ast_exception("missing constant value");
                token = lex->get_token();
                scoped_symbol_table_layer.front().symbol_table->insert({identifier, token_to_type(type)});
                constant_table.emplace(identifier, std::move(parse_expression(parse_primary_expression(false), false)));
                lhs = std::make_unique<VariableExprAST>(std::move(identifier));
            } while (token == ',');
            break;
        }
        case TOKEN_IDENTIFIER: {
            std::string identifier = std::move(lex->identifier);
            constexpr Token type = TOKEN_TYPE_INT;
            token = lex->get_token();
            switch (token) {
                case TOKEN_TYPE_FLOAT:
                    if (!is_variable(identifier)) {
                        scoped_symbol_table_layer.back().symbol_table->insert({identifier, SYMBOL_TYPE_FLOAT});
                    }
                    token = lex->get_token();
                    break;
                case TOKEN_TYPE_STRING:
                    if (!is_variable(identifier)) {
                        scoped_symbol_table_layer.back().symbol_table->insert({identifier, SYMBOL_TYPE_STRING});
                    }
                    token = lex->get_token();
                    break;
                case TOKEN_TYPE_POINTER:
                    if (!is_variable(identifier)) {
                        scoped_symbol_table_layer.back().symbol_table->insert({identifier, SYMBOL_TYPE_POINTER});
                    }
                    token = lex->get_token();
                    break;
                case TOKEN_TYPE_INT:
                    if (!is_variable(identifier)) {
                        scoped_symbol_table_layer.back().symbol_table->insert({identifier, SYMBOL_TYPE_INT});
                    }
                    token = lex->get_token();
                default:
                    if (token == '=') {
                        const int symbol_type = is_variable(identifier);
                        if (symbol_type == 0) {
                            scoped_symbol_table_layer.back().symbol_table->insert({identifier, SYMBOL_TYPE_INT});
                        } else if (symbol_type != token_to_type(type)) {
                            throw ast_exception("mismatched variable type");
                        }
                        break;
                    }
                    if (token == '(' || function_first) {
                        // must be function call
                        lhs = parse_call_expression(std::move(identifier));
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
            lhs = std::move(parse_expression(std::move(parse_primary_expression(function_first)), function_first));
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
                UnaryExprAST>('-', std::move(parse_primary_expression(function_first)));
            break;
        case TOKEN_LOGIC_NOT:
            token = lex->get_token();
            lhs = std::make_unique<UnaryExprAST>(TOKEN_LOGIC_NOT, std::move(parse_primary_expression(function_first)));
            break;
        case TOKEN_RETURN:
            token = lex->get_token();
            if (token == TOKEN_EOF || is_end_of_stmt(token)) {
                lhs = std::make_unique<ReturnExprAST>(nullptr);
                break;
            }
            lhs = std::make_unique<ReturnExprAST>(std::move(
                parse_expression(std::move(parse_primary_expression(false)), false)));
            break;
        case TOKEN_IF:
            lhs = parse_if_statement();
            break;
        case TOKEN_WHILE:
            lhs = parse_while_statement();
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
    if (token == TOKEN_TYPE_INT || token == TOKEN_TYPE_FLOAT ||
        token == TOKEN_TYPE_STRING || token == TOKEN_TYPE_POINTER) {
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
            default_value = parse_expression(parse_primary_expression(false), false);
            optional_args++;
        } else {
            mandatory_args++;
        }
        function->symbol_table.insert({arg_name, type});
        function->arguments.push_back(
            std::make_unique<FunctionArgument>(std::move(arg_name), type, std::move(default_value)));
    } while (token == ',');
    if (token != ')') throw ast_exception("expecting closing parenthesis");
    scoped_symbol_table_layer.front().symbol_table->insert({non_case_name, SYMBOL_TYPE_FUNCTION});
    if (is_extern) {
        if (extern_function_table.contains(name)) throw ast_exception("duplicate extern function");
        extern_function_table.emplace(non_case_name, std::move(function));
        return nullptr;
    }

    // looking for duplicate signatures...
    auto [first, second] = function_table.equal_range(function->name);
    for (auto& it = first; it != second; ++it) {
        const size_t define_mandatory_args = std::ranges::count_if(it->second->signature->arguments,
                                                                   [](const std::unique_ptr<FunctionArgument>& arg) {
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
    scoped_symbol_table_layer.emplace_back(&function->signature->symbol_table, SYMBOL_TABLE_TYPE_FUNCTION);
    this->token = lex->get_token();
    do {
        if (token == TOKEN_EOF) throw ast_exception("expecting end function");
        if (token == TOKEN_FUNCTION) throw ast_exception("cannot define function in function");
        if (token == TOKEN_EXTERN) throw ast_exception("cannot define extern function in function");
        if (token == TOKEN_CONST) throw ast_exception("cannot define constant in function");
        if (token == TOKEN_GLOBAL) throw ast_exception("cannot define global in function");
        if (token == TOKEN_EXIT || token == TOKEN_CONTINUE)
            throw ast_exception(
                "exit or continue can be used in a while statement only");
        if (token == TOKEN_END && (this->token = lex->get_token()) == TOKEN_FUNCTION) { break; }
        if (is_end_of_stmt(token)) {
            this->token = lex->get_token();
            continue;
        }
        std::unique_ptr<ExprAST> lhs = std::move(parse_primary_expression());
        function->body.push_back(std::move(parse_expression(std::move(lhs))));
    } while (true);
    function_table.emplace(function->signature->name, std::move(function));
    scoped_symbol_table_layer.pop_back();
}

std::unique_ptr<IfStatementAST> AST::parse_if_statement() {
    this->token = lex->get_token();
    std::unique_ptr<ExprAST> condition = parse_expression(parse_primary_expression(false), false);
    if (this->token == TOKEN_THEN) this->token = lex->get_token();
    std::unique_ptr<IfStatementAST> statement = std::make_unique<IfStatementAST>(std::move(condition));
    if (token != TOKEN_LINE_FEED) {
        scoped_symbol_table_layer.emplace_back(&statement->statement_true_symbol_table, SYMBOL_TABLE_TYPE_IF);
        do {
            if (this->token == TOKEN_ELSE) {
                this->token = lex->get_token();
                scoped_symbol_table_layer.emplace_back(&statement->statement_false_symbol_table, SYMBOL_TABLE_TYPE_IF);
                do {
                    statement->statement_false.push_back(parse_expression(parse_primary_expression()));
                    if (token == TOKEN_COLON) this->token = lex->get_token();
                } while (token != TOKEN_LINE_FEED && token != TOKEN_EOF);
                scoped_symbol_table_layer.pop_back();
                break;
            }
            if (this->token == TOKEN_ELSE_IF) {
                scoped_symbol_table_layer.emplace_back(&statement->statement_false_symbol_table, SYMBOL_TABLE_TYPE_IF);
                statement->statement_false.push_back(parse_if_statement());
                scoped_symbol_table_layer.pop_back();
                break;
            }
            if (token == TOKEN_EXIT || token == TOKEN_CONTINUE) {
                const size_t while_count = std::count_if(scoped_symbol_table_layer.rbegin(), scoped_symbol_table_layer.rend(),
                                                      [](SymbolTableLayer layer) {
                                                          return layer.layer_type == SYMBOL_TABLE_TYPE_WHILE;
                                                      });
                if (while_count == 0) throw ast_exception("exit or continue can be used in a while statement only");
                if (token == TOKEN_EXIT) {
                    statement->statement_true.push_back(std::make_unique<ExitAST>());
                }
                if (token == TOKEN_CONTINUE) {
                    statement->statement_true.push_back(std::make_unique<ContinueAST>());
                }
                this->token = lex->get_token();
                continue;
            }
            statement->statement_true.push_back(parse_expression(parse_primary_expression()));
            if (token == TOKEN_COLON) this->token = lex->get_token();
        } while (token != TOKEN_LINE_FEED && token != TOKEN_EOF);
        scoped_symbol_table_layer.pop_back();
        return statement;
    }
    scoped_symbol_table_layer.emplace_back(&statement->statement_true_symbol_table, SYMBOL_TABLE_TYPE_IF);
    do {
        if (token == TOKEN_EOF) throw ast_exception("expecting endif");
        if (token == TOKEN_FUNCTION) throw ast_exception("cannot define function in if statement");
        if (token == TOKEN_EXTERN) throw ast_exception("cannot define extern function in if statement");
        if (token == TOKEN_CONST) throw ast_exception("cannot define constant in if statement");
        if (token == TOKEN_GLOBAL) throw ast_exception("cannot define global in if statement");
        if (token == TOKEN_EXIT || token == TOKEN_CONTINUE) {
            const size_t while_count = std::count_if(scoped_symbol_table_layer.rbegin(), scoped_symbol_table_layer.rend(),
                                                  [](SymbolTableLayer layer) {
                                                      return layer.layer_type == SYMBOL_TABLE_TYPE_WHILE;
                                                  });
            if (while_count == 0) throw ast_exception("exit or continue can be used in a while statement only");
            if (token == TOKEN_EXIT) {
                statement->statement_true.push_back(std::make_unique<ExitAST>());
            }
            if (token == TOKEN_CONTINUE) {
                statement->statement_true.push_back(std::make_unique<ContinueAST>());
            }
            this->token = lex->get_token();
            continue;
        }
        if (token == TOKEN_END && (this->token = lex->get_token()) == TOKEN_IF) { break; }
        if (is_end_of_stmt(token)) {
            this->token = lex->get_token();
            continue;
        }
        statement->statement_true.push_back(parse_expression(parse_primary_expression()));
    } while (this->token != TOKEN_ELSE && this->token != TOKEN_ELSE_IF && this->token != TOKEN_ENDIF);
    scoped_symbol_table_layer.pop_back();
    if (this->token == TOKEN_ELSE_IF) {
        scoped_symbol_table_layer.emplace_back(&statement->statement_false_symbol_table, SYMBOL_TABLE_TYPE_IF);
        statement->statement_false.push_back(parse_if_statement());
        scoped_symbol_table_layer.pop_back();
    } else {
        if (this->token == TOKEN_ELSE) {
            scoped_symbol_table_layer.emplace_back(&statement->statement_false_symbol_table, SYMBOL_TABLE_TYPE_IF);
            this->token = lex->get_token();
            do {
                if (token == TOKEN_EOF) throw ast_exception("expecting endif");
                if (token == TOKEN_FUNCTION) throw ast_exception("cannot define function in if statement");
                if (token == TOKEN_EXTERN) throw ast_exception("cannot define extern function in if statement");
                if (token == TOKEN_CONST) throw ast_exception("cannot define constant in if statement");
                if (token == TOKEN_GLOBAL) throw ast_exception("cannot define global in if statement");
                if (token == TOKEN_EXIT || token == TOKEN_CONTINUE) {
                    const size_t while_count = std::count_if(scoped_symbol_table_layer.rbegin(),
                                                          scoped_symbol_table_layer.rend(),
                                                          [](SymbolTableLayer layer) {
                                                              return layer.layer_type == SYMBOL_TABLE_TYPE_WHILE;
                                                          });
                    if (while_count == 0) throw ast_exception("exit or continue can be used in a while statement only");
                    if (token == TOKEN_EXIT) {
                        statement->statement_true.push_back(std::make_unique<ExitAST>());
                    }
                    if (token == TOKEN_CONTINUE) {
                        statement->statement_true.push_back(std::make_unique<ContinueAST>());
                    }
                    this->token = lex->get_token();
                    continue;
                }
                if (token == TOKEN_END && (this->token = lex->get_token()) == TOKEN_IF) { break; }
                if (is_end_of_stmt(token)) {
                    this->token = lex->get_token();
                    continue;
                }
                statement->statement_false.push_back(parse_expression(parse_primary_expression()));
            } while (this->token != TOKEN_ELSE && this->token != TOKEN_ELSE_IF && this->token != TOKEN_ENDIF);
            scoped_symbol_table_layer.pop_back();
        }

        if (this->token != TOKEN_ENDIF) {
            if (this->token != TOKEN_END || (this->token = lex->get_token()) != TOKEN_IF)
                throw ast_exception("expecting endif");
        }
        this->token = lex->get_token();
    }
    return statement;
}

std::unique_ptr<WhileStatementAST> AST::parse_while_statement() {
    this->token = lex->get_token();
    std::unique_ptr<ExprAST> condition = parse_expression(parse_primary_expression(false), false);
    if (this->token == TOKEN_THEN) this->token = lex->get_token();
    std::unique_ptr<WhileStatementAST> statement = std::make_unique<WhileStatementAST>(std::move(condition));
    if (token != TOKEN_LINE_FEED) {
        scoped_symbol_table_layer.emplace_back(&statement->statement_true_symbol_table, SYMBOL_TABLE_TYPE_WHILE);
        do {
            statement->statement_true.push_back(parse_expression(parse_primary_expression()));
            if (token == TOKEN_COLON) this->token = lex->get_token();
        } while (token != TOKEN_LINE_FEED && token != TOKEN_EOF);
        scoped_symbol_table_layer.pop_back();
        return statement;
    }
    scoped_symbol_table_layer.emplace_back(&statement->statement_true_symbol_table, SYMBOL_TABLE_TYPE_WHILE);
    do {
        if (token == TOKEN_EOF) throw ast_exception("expecting wend");
        if (token == TOKEN_FUNCTION) throw ast_exception("cannot define function in while statement");
        if (token == TOKEN_EXTERN) throw ast_exception("cannot define extern function in while statement");
        if (token == TOKEN_CONST) throw ast_exception("cannot define constant in while statement");
        if (token == TOKEN_GLOBAL) throw ast_exception("cannot define global in while statement");
        if (token == TOKEN_EXIT) {
            statement->statement_true.push_back(std::make_unique<ExitAST>());
            this->token = lex->get_token();
            continue;
        }
        if (token == TOKEN_CONTINUE) {
            statement->statement_true.push_back(std::make_unique<ContinueAST>());
            this->token = lex->get_token();
            continue;
        }
        if (is_end_of_stmt(token)) {
            this->token = lex->get_token();
            continue;
        }
        statement->statement_true.push_back(parse_expression(parse_primary_expression()));
    } while (this->token != TOKEN_WEND);
    scoped_symbol_table_layer.pop_back();
    this->token = lex->get_token();
    return statement;
}

std::unique_ptr<CallExprAST> AST::parse_call_expression(std::string callee) {
    std::vector<std::unique_ptr<ExprAST>> arguments = {};
    if (token == ')') return std::make_unique<CallExprAST>(std::move(callee), std::move(arguments));
    do {
        if (token == ',' || token == '(') this->token = lex->get_token();
        if (token == ')' || token == TOKEN_EOF || is_end_of_stmt(token)) {
            this->token = lex->get_token();
            break;
        }
        std::unique_ptr<ExprAST> lhs = std::move(parse_primary_expression(false));
        arguments.push_back(std::move(parse_expression(std::move(lhs), false)));
    } while (token == ',');
    if (token == ')') this->token = lex->get_token();
    return std::make_unique<CallExprAST>(std::move(callee), std::move(arguments));
}

std::unique_ptr<ExprAST> AST::parse_expression(std::unique_ptr<ExprAST> lhs, bool function_first) {
    while (true) {
        int op = token;
        if (token == TOKEN_EOF || is_end_of_stmt(token) || token == TOKEN_THEN || token == TOKEN_ELSE || token ==
            TOKEN_ELSE_IF || token == ')' || token == ',')
            return lhs;

        token = lex->get_token();
        std::unique_ptr<ExprAST> rhs = std::move(
            parse_primary_expression(op == '=' ? false : function_first));

        while (token != TOKEN_EOF && !is_end_of_stmt(token) && token != TOKEN_THEN && token != TOKEN_ELSE && token
               != TOKEN_ELSE_IF && token != ')' && token != ',') {
            if (op_precedence.at(op) < op_precedence.at(token)) {
                int next_op = token;
                token = lex->get_token();
                rhs = std::make_unique<BinaryExprAST>(next_op, std::move(rhs),
                                                      parse_expression(
                                                          std::move(parse_primary_expression(
                                                              op == '=' ? false : function_first)),
                                                          op == '=' ? false : function_first));
            }
        }

        lhs = std::make_unique<BinaryExprAST>(op == '=' ? (function_first ? '=' : TOKEN_EQUALS) : op, std::move(lhs),
                                              std::move(rhs));
    }
}

int AST::is_variable(const std::string& name) {
    for (const auto& it : std::ranges::reverse_view(scoped_symbol_table_layer)) {
        if (const int type = is_variable(it.symbol_table, name); type != 0) return type;
    }
    return 0;
}

int AST::is_variable(SymbolTable* symbol_table, const std::string& name) {
    if (!symbol_table->contains(name)) return 0;
    const auto [first, second] = symbol_table->equal_range(name);
    for (auto it = first; it != second; ++it) {
        if (is_variable_type(it->second)) return it->second;
    }
    return 0;
}
