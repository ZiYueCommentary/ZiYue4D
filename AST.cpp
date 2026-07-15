#include "AST.h"
#include <algorithm>
#include <ranges>

const std::unordered_map<int, int> op_precedence = {
    {'=', 10},
    {TOKEN_NOT_EQUALS, 20},
    {'+', 30}, {'-', 30}, {'*', 31}, {'/', 31}
};

bool AST::parse() {
    bool occur_errors = false;
    scoped_symbol_table_layer.emplace_back(new SymbolTable{}, SYMBOL_TABLE_TYPE_GLOBAL); // global
    scoped_symbol_table_layer.front().symbol_table->insert({"main", SYMBOL_TYPE_FUNCTION});
    auto signature = std::make_unique<FunctionSignatureAST>("main", SYMBOL_TYPE_INT, llvm::SMRange());
    signature->arguments.push_back({std::make_unique<FunctionArgument>("__argc", SYMBOL_TYPE_INT, nullptr)});
    signature->arguments.push_back({std::make_unique<FunctionArgument>("__argv", SYMBOL_TYPE_POINTER, nullptr)});
    signature->symbol_table.emplace("__argc", SYMBOL_TYPE_INT);
    signature->symbol_table.emplace("__argv", SYMBOL_TYPE_POINTER);
    auto function = std::make_unique<FunctionAST>(std::move(signature), llvm::SMRange());
    scoped_symbol_table_layer.emplace_back(&function->signature->symbol_table); // main local
    function_table.emplace("main", std::move(function));
    const auto& main = function_table.equal_range("main").first->second;
    while (true) {
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
        if (token == TOKEN_EXIT || token == TOKEN_CONTINUE) {
            lex->source_mgr.PrintMessage(lex->range.Start, llvm::SourceMgr::DK_Error,
                                         "exit or continue can be used in while statements only", lex->range);
            move_to_next_stmt();
            continue;
        }
        std::unique_ptr<ExprAST> lhs = std::move(parse_primary_expression());
        main->body.push_back(std::move(parse_expression(std::move(lhs))));
    }
    scoped_symbol_table_layer.pop_back(); // main local

    return occur_errors;
}

std::unique_ptr<ExprAST> AST::parse_primary_expression(bool function_first) {
    std::unique_ptr<ExprAST> lhs = nullptr;
    switch (token) {
        case TOKEN_GLOBAL:
            if (scoped_symbol_table_layer.size() > 2) {
                lex->source_mgr.PrintMessage(lex->quick_build_loc(lex->last_token_pos), llvm::SourceMgr::DK_Error,
                                             "global cannot be defined in function",
                                             lex->range, {
                                                 llvm::SMFixIt(lex->quick_build_loc(lex->last_token_pos),
                                                               "move declaration outside the function")
                                             });
                throw std::exception();
            }
            do {
                token = lex->get_token();
                std::string identifier = std::move(lex->identifier);
                const llvm::SMRange range = lex->range;
                if (is_variable(scoped_symbol_table_layer.front().symbol_table, identifier)) {
                    lex->source_mgr.PrintMessage(lex->quick_build_loc(lex->last_token_pos), llvm::SourceMgr::DK_Error,
                                                 "duplicate global variable definition",
                                                 range, {
                                                     llvm::SMFixIt(lex->quick_build_loc(lex->last_token_pos),
                                                                   "rename variable")
                                                 });
                    throw std::exception();
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
                lhs = std::make_unique<VariableExprAST>(std::move(identifier), range);
            } while (token == ',');
            break;
        case TOKEN_LOCAL:
            do {
                token = lex->get_token();
                if (is_variable(lex->identifier)) {
                    lex->source_mgr.PrintMessage(lex->quick_build_loc(lex->last_token_pos), llvm::SourceMgr::DK_Error,
                                                 "duplicate variable definition",
                                                 lex->range, {
                                                     llvm::SMFixIt(lex->quick_build_loc(lex->last_token_pos),
                                                                   "rename variable")
                                                 });
                    throw std::exception();
                }
                lhs = parse_expression(parse_primary_expression(function_first), function_first);
            } while (token == ',');
            break;
        case TOKEN_CONST: {
            if (scoped_symbol_table_layer.size() > 2) {
                lex->source_mgr.PrintMessage(lex->quick_build_loc(lex->last_token_pos), llvm::SourceMgr::DK_Error,
                                             "constant cannot be defined in function",
                                             lex->range, {
                                                 llvm::SMFixIt(lex->quick_build_loc(lex->last_token_pos),
                                                               "move declaration outside the function")
                                             });
                throw std::exception();
            }

            do {
                token = lex->get_token();
                std::string identifier = std::move(lex->identifier);
                const llvm::SMRange range = lex->range;
                if (is_variable(scoped_symbol_table_layer.front().symbol_table, identifier)) {
                    lex->source_mgr.PrintMessage(lex->quick_build_loc(lex->last_token_pos), llvm::SourceMgr::DK_Error,
                                                 "duplicate constant definition",
                                                 range, {
                                                     llvm::SMFixIt(lex->quick_build_loc(lex->last_token_pos),
                                                                   "rename constant")
                                                 });
                    throw std::exception();
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
                    default: {
                        lex->source_mgr.PrintMessage(lex->quick_build_loc(lex->last_token_pos),
                                                     llvm::SourceMgr::DK_Error,
                                                     "cannot define " + symbol_type_to_literal(
                                                         token_to_type(static_cast<Token>(token))) + " constant",
                                                     range);
                        throw std::exception();
                    }
                }

                if (token != '=') {
                    lex->source_mgr.PrintMessage(lex->quick_build_loc(lex->last_token_pos), llvm::SourceMgr::DK_Error,
                                                 "expecting constant value", range);
                    throw std::exception();
                }
                token = lex->get_token();
                scoped_symbol_table_layer.front().symbol_table->insert({identifier, token_to_type(type)});
                constant_table.emplace(identifier, std::move(parse_expression(parse_primary_expression(false), false)));
                lhs = std::make_unique<VariableExprAST>(std::move(identifier), range);
            } while (token == ',');
            break;
        }
        case TOKEN_IDENTIFIER: {
            std::string identifier = std::move(lex->identifier);
            const llvm::SMRange range = lex->range;
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
                            lex->source_mgr.PrintMessage(range.End,
                                                         llvm::SourceMgr::DK_Error,
                                                         "mismatched variable type flag, expecting " +
                                                         symbol_type_to_literal(static_cast<SymbolType>(symbol_type)) +
                                                         ", found " + symbol_type_to_literal(token_to_type(type)),
                                                         range);
                            throw std::exception();
                        }
                        break;
                    }
                    if (token == '(' || function_first) {
                        // must be function call
                        lhs = parse_call_expression(std::move(identifier), lex->range.Start);
                        return lhs;
                    }
            }

            lhs = std::make_unique<VariableExprAST>(std::move(identifier), range);
            break;
        }
        case TOKEN_INTEGER:
            lhs = std::make_unique<IntegerExprAST>(lex->int_value, lex->range);
            token = lex->get_token();
            break;
        case TOKEN_FLOAT:
            lhs = std::make_unique<FloatExprAST>(lex->float_value, lex->range);
            token = lex->get_token();
            break;
        case TOKEN_STRING:
            lhs = std::make_unique<StringExprAST>(std::move(lex->string_value), lex->range);
            token = lex->get_token();
            break;
        case '(':
            token = lex->get_token();
            lhs = std::move(parse_expression(std::move(parse_primary_expression(function_first)), function_first));
            if (token != ')') {
                lex->source_mgr.PrintMessage(lex->quick_build_loc(lex->last_token_pos),
                                             llvm::SourceMgr::DK_Error,
                                             "expecting ')'", lhs->range, {
                                                 llvm::SMFixIt(lex->quick_build_loc(lex->last_token_pos),
                                                               "adding ')' in the end")
                                             });
                throw std::exception();
            }
            token = lex->get_token();
            break;
        case '&': {
            const size_t begin = lex->pos;
            token = lex->get_token();
            if (token == TOKEN_IDENTIFIER) {
                lhs = std::make_unique<UnaryExprAST>(
                    '&', std::move(std::make_unique<VariableExprAST>(std::move(lex->identifier), lex->range)),
                    lex->quick_build_range(begin, lex->pos));
                token = lex->get_token();
            } else {
                lex->source_mgr.PrintMessage(lex->quick_build_loc(lex->last_token_pos),
                                             llvm::SourceMgr::DK_Error,
                                             "expecting identifier when retrieving pointer",
                                             lex->quick_build_range(begin, lex->pos));
                throw std::exception();
            }
            break;
        }
        case '-': {
            const size_t begin = lex->pos;
            token = lex->get_token();
            lhs = std::make_unique<
                UnaryExprAST>('-', std::move(parse_primary_expression(function_first)),
                              lex->quick_build_range(begin, lex->pos));
            break;
        }
        case TOKEN_LOGIC_NOT: {
            const size_t begin = lex->pos;
            token = lex->get_token();
            lhs = std::make_unique<UnaryExprAST>(TOKEN_LOGIC_NOT, std::move(parse_primary_expression(function_first)),
                                                 lex->quick_build_range(begin, lex->pos));
            break;
        }
        case TOKEN_RETURN: {
            const size_t begin = lex->pos;
            token = lex->get_token();
            if (token == TOKEN_EOF || is_end_of_stmt(token)) {
                lhs = std::make_unique<ReturnExprAST>(nullptr,
                                                      lex->quick_build_range(begin, lex->pos));
                break;
            }
            lhs = std::make_unique<ReturnExprAST>(std::move(
                                                      parse_expression(std::move(parse_primary_expression(false)),
                                                                       false)),
                                                  lex->quick_build_range(begin, lex->pos));
            break;
        }
        case TOKEN_IF:
            lhs = parse_if_statement();
            break;
        case TOKEN_WHILE:
            lhs = parse_while_statement();
            break;
        default:
            lex->source_mgr.PrintMessage(lex->quick_build_loc(lex->last_token_pos),
                                         llvm::SourceMgr::DK_Error,
                                         "expecting primary expression",
                                         lex->quick_build_range(lex->last_token_pos, lex->pos));
            throw std::exception();
    }
    return lhs;
}

std::unique_ptr<FunctionSignatureAST> AST::parse_function_signature(bool is_extern) {
    const size_t begin = lex->pos;
    this->token = lex->get_token();
    if (token != TOKEN_IDENTIFIER) {
        lex->source_mgr.PrintMessage(lex->range.Start, llvm::SourceMgr::DK_Error,
                                     "expecting function name", lex->range);
        throw std::exception();
    }
    std::string name = std::move(lex->case_identifier);
    std::string non_case_name = std::move(lex->identifier);
    this->token = lex->get_token();
    SymbolType return_value_type = is_extern ? SYMBOL_TYPE_VOID : SYMBOL_TYPE_INT;
    if (token == TOKEN_TYPE_INT || token == TOKEN_TYPE_FLOAT ||
        token == TOKEN_TYPE_STRING || token == TOKEN_TYPE_POINTER) {
        return_value_type = token_to_type(static_cast<Token>(token));
        this->token = lex->get_token();
    }
    if (token != '(') {
        lex->source_mgr.PrintMessage(lex->quick_build_loc(lex->last_token_pos), llvm::SourceMgr::DK_Error,
                                     "expecting '('", lex->range);
        throw std::exception();
    }
    auto function = std::make_unique<FunctionSignatureAST>(name, return_value_type,
                                                           lex->quick_build_range(begin, lex->pos));
    int mandatory_args = 0, optional_args = 0;
    do {
        this->token = lex->get_token();
        if (token == ')') break;
        if (token != TOKEN_IDENTIFIER) {
            lex->source_mgr.PrintMessage(lex->range.Start, llvm::SourceMgr::DK_Error,
                                         "expecting argument name", lex->range);
            throw std::exception();
        }
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
        function->symbol_table.emplace(arg_name, type);
        function->arguments.push_back(
            std::make_unique<FunctionArgument>(std::move(arg_name), type, std::move(default_value)));
    } while (token == ',');
    if (token != ')') {
        lex->source_mgr.PrintMessage(lex->quick_build_loc(lex->last_token_pos), llvm::SourceMgr::DK_Error,
                                     "expecting ')'", lex->range);
        throw std::exception();
    }
    scoped_symbol_table_layer.front().symbol_table->insert({non_case_name, SYMBOL_TYPE_FUNCTION});
    if (is_extern) {
        if (extern_function_table.contains(name)) {
            lex->source_mgr.PrintMessage(lex->range.Start, llvm::SourceMgr::DK_Error, "duplicate extern function",
                                         lex->range);
            throw std::exception();
        }
        extern_function_table.emplace(non_case_name, std::move(function));
        return nullptr;
    }

    // looking for duplicate signatures...
    auto [first, second] = function_table.equal_range(non_case_name);
    for (auto& it = first; it != second; ++it) {
        const size_t define_mandatory_args = std::ranges::count_if(it->second->signature->arguments,
                                                                   [](const std::unique_ptr<FunctionArgument>& arg) {
                                                                       return arg->default_value == nullptr;
                                                                   });
        const size_t define_optional_args = it->second->signature->arguments.size() - define_mandatory_args;
        if (define_mandatory_args == mandatory_args && define_optional_args == optional_args) {
            lex->source_mgr.PrintMessage(lex->quick_build_loc(begin), llvm::SourceMgr::DK_Error,
                                         "duplicate function signature",
                                         lex->quick_build_range(begin, lex->last_token_pos));
            throw std::exception();
        }
    }

    return function;
}

void AST::parse_function_definition() {
    const size_t begin = lex->pos;
    auto function = std::make_unique<FunctionAST>(std::move(parse_function_signature()),
                                                  lex->quick_build_range(begin, lex->pos));
    scoped_symbol_table_layer.emplace_back(&function->signature->symbol_table, SYMBOL_TABLE_TYPE_FUNCTION);
    this->token = lex->get_token();
    do {
        general_token_check("function", "end function");
        if (token == TOKEN_END && (this->token = lex->get_token()) == TOKEN_FUNCTION) { break; }
        if (is_end_of_stmt(token)) {
            this->token = lex->get_token();
            continue;
        }
        std::unique_ptr<ExprAST> lhs = std::move(parse_primary_expression());
        function->body.push_back(std::move(parse_expression(std::move(lhs))));
    } while (true);
    function_table.emplace(lex->to_lower_string(function->signature->name), std::move(function));
    scoped_symbol_table_layer.pop_back();
}

std::unique_ptr<IfStatementAST> AST::parse_if_statement() {
    const size_t begin = lex->pos;
    this->token = lex->get_token();
    std::unique_ptr<ExprAST> condition = parse_expression(parse_primary_expression(false), false);
    if (this->token == TOKEN_THEN) this->token = lex->get_token();
    std::unique_ptr<IfStatementAST> statement = std::make_unique<IfStatementAST>(
        std::move(condition), lex->quick_build_range(begin, lex->pos));
    if (token != TOKEN_LINE_FEED) {
        scoped_symbol_table_layer.emplace_back(&statement->statement_true_symbol_table, SYMBOL_TABLE_TYPE_BRANCH);
        do {
            general_token_check("if statement", "endif");
            if (this->token == TOKEN_ELSE) {
                this->token = lex->get_token();
                scoped_symbol_table_layer.emplace_back(&statement->statement_false_symbol_table,
                                                       SYMBOL_TABLE_TYPE_BRANCH);
                do {
                    statement->statement_false.push_back(parse_expression(parse_primary_expression()));
                    if (token == TOKEN_COLON) this->token = lex->get_token();
                } while (token != TOKEN_LINE_FEED && token != TOKEN_EOF);
                scoped_symbol_table_layer.pop_back();
                break;
            }
            if (this->token == TOKEN_ELSE_IF) {
                scoped_symbol_table_layer.emplace_back(&statement->statement_false_symbol_table,
                                                       SYMBOL_TABLE_TYPE_BRANCH);
                statement->statement_false.push_back(parse_if_statement());
                scoped_symbol_table_layer.pop_back();
                break;
            }
            if (token == TOKEN_EXIT) {
                statement->statement_true.push_back(std::make_unique<ExitAST>(lex->range));
                this->token = lex->get_token();
                continue;
            }
            if (token == TOKEN_CONTINUE) {
                statement->statement_true.push_back(std::make_unique<ContinueAST>(lex->range));
                this->token = lex->get_token();
                continue;
            }
            statement->statement_true.push_back(parse_expression(parse_primary_expression()));
            if (token == TOKEN_COLON) this->token = lex->get_token();
        } while (token != TOKEN_LINE_FEED && token != TOKEN_EOF);
        scoped_symbol_table_layer.pop_back();
        return statement;
    }
    scoped_symbol_table_layer.emplace_back(&statement->statement_true_symbol_table, SYMBOL_TABLE_TYPE_BRANCH);
    do {
        general_token_check("if statement", "endif");
        if (token == TOKEN_EXIT) {
            statement->statement_true.push_back(std::make_unique<ExitAST>(lex->range));
            this->token = lex->get_token();
            continue;
        }
        if (token == TOKEN_CONTINUE) {
            statement->statement_true.push_back(std::make_unique<ContinueAST>(lex->range));
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
        scoped_symbol_table_layer.emplace_back(&statement->statement_false_symbol_table, SYMBOL_TABLE_TYPE_BRANCH);
        statement->statement_false.push_back(parse_if_statement());
        scoped_symbol_table_layer.pop_back();
    } else {
        if (this->token == TOKEN_ELSE) {
            scoped_symbol_table_layer.emplace_back(&statement->statement_false_symbol_table, SYMBOL_TABLE_TYPE_BRANCH);
            this->token = lex->get_token();
            do {
                general_token_check("if statement", "endif", true);
                if (token == TOKEN_EXIT) {
                    statement->statement_true.push_back(std::make_unique<ExitAST>(lex->range));
                    this->token = lex->get_token();
                    continue;
                }
                if (token == TOKEN_CONTINUE) {
                    statement->statement_true.push_back(std::make_unique<ContinueAST>(lex->range));
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
            if (this->token != TOKEN_END || (this->token = lex->get_token()) != TOKEN_IF) {
                lex->source_mgr.PrintMessage(lex->range.Start, llvm::SourceMgr::DK_Error, "expecting endif",
                                             lex->range);
                throw std::exception();
            }
        }
        this->token = lex->get_token();
    }
    return statement;
}

std::unique_ptr<WhileStatementAST> AST::parse_while_statement() {
    const size_t begin = lex->pos;
    this->token = lex->get_token();
    std::unique_ptr<ExprAST> condition = parse_expression(parse_primary_expression(false), false);
    if (this->token == TOKEN_THEN) this->token = lex->get_token();
    auto statement = std::make_unique<WhileStatementAST>(std::move(condition), lex->quick_build_range(begin, lex->pos));
    if (token != TOKEN_LINE_FEED) {
        scoped_symbol_table_layer.emplace_back(&statement->statement_true_symbol_table, SYMBOL_TABLE_TYPE_LOOP);
        do {
            general_token_check("while statement", "wend", true);
            statement->statement_true.push_back(parse_expression(parse_primary_expression()));
            if (token == TOKEN_COLON) this->token = lex->get_token();
        } while (token != TOKEN_LINE_FEED && token != TOKEN_EOF);
        scoped_symbol_table_layer.pop_back();
        return statement;
    }
    scoped_symbol_table_layer.emplace_back(&statement->statement_true_symbol_table, SYMBOL_TABLE_TYPE_LOOP);
    do {
        general_token_check("while statement", "wend");
        if (token == TOKEN_EXIT) {
            statement->statement_true.push_back(std::make_unique<ExitAST>(lex->range));
            this->token = lex->get_token();
            continue;
        }
        if (token == TOKEN_CONTINUE) {
            statement->statement_true.push_back(std::make_unique<ContinueAST>(lex->range));
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

void AST::general_token_check(const std::string& stmt_name, const std::string& stmt_end, const bool single_line) const {
    if (token == TOKEN_EOF && !single_line) {
        lex->source_mgr.PrintMessage(lex->range.Start, llvm::SourceMgr::DK_Error, "expecting " + stmt_end, lex->range);
        throw std::exception();
    }
    if (token == TOKEN_FUNCTION) {
        lex->source_mgr.PrintMessage(lex->range.Start, llvm::SourceMgr::DK_Error,
                                     "cannot define function in " + stmt_name, lex->range);
        throw std::exception();
    }
    if (token == TOKEN_EXTERN) {
        lex->source_mgr.PrintMessage(lex->range.Start, llvm::SourceMgr::DK_Error,
                                     "cannot define extern function in " + stmt_name, lex->range);
        throw std::exception();
    }
    if (token == TOKEN_CONST) {
        lex->source_mgr.PrintMessage(lex->range.Start, llvm::SourceMgr::DK_Error,
                                     "cannot define constant in " + stmt_name, lex->range);
        throw std::exception();
    }
    if (token == TOKEN_GLOBAL) {
        lex->source_mgr.PrintMessage(lex->range.Start, llvm::SourceMgr::DK_Error,
                                     "cannot define global in " + stmt_name, lex->range);
        throw std::exception();
    }
    if (token == TOKEN_EXIT || token == TOKEN_CONTINUE) {
        const size_t while_count = std::count_if(scoped_symbol_table_layer.rbegin(),
                                                 scoped_symbol_table_layer.rend(),
                                                 [](const SymbolTableLayer layer) {
                                                     return layer.layer_type == SYMBOL_TABLE_TYPE_LOOP;
                                                 });
        if (while_count != 0) return;
        lex->source_mgr.PrintMessage(lex->range.Start, llvm::SourceMgr::DK_Error,
                                     "exit or continue can be used in a while statement only", lex->range);
        throw std::exception();
    }
}

std::unique_ptr<CallExprAST> AST::parse_call_expression(std::string callee, const llvm::SMLoc start) {
    std::vector<std::unique_ptr<ExprAST>> arguments = {};
    if (token == ')')
        return std::make_unique<CallExprAST>(std::move(callee), std::move(arguments),
                                             llvm::SMRange(start, lex->quick_build_loc(lex->pos)));
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
    return std::make_unique<CallExprAST>(std::move(callee), std::move(arguments),
                                         llvm::SMRange(start, lex->quick_build_loc(lex->pos)));
}

std::unique_ptr<ExprAST> AST::parse_expression(std::unique_ptr<ExprAST> lhs, bool function_first) {
    while (true) {
        try {
            int op = token;
            const size_t op_loc = lex->last_token_pos;
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
                    const size_t next_op_loc = lex->last_token_pos;
                    token = lex->get_token();
                    rhs = std::make_unique<BinaryExprAST>(next_op, std::move(rhs),
                                                          parse_expression(
                                                              std::move(parse_primary_expression(
                                                                  op == '=' ? false : function_first)),
                                                              op == '=' ? false : function_first),
                                                          llvm::SMRange(
                                                              lhs->range.Start, lex->quick_build_loc(lex->pos)),
                                                          lex->quick_build_loc(next_op_loc));
                }
            }

            lhs = std::make_unique<BinaryExprAST>(op == '=' ? (function_first ? '=' : TOKEN_EQUALS) : op,
                                                  std::move(lhs),
                                                  std::move(rhs),
                                                  llvm::SMRange(lhs->range.Start, lex->quick_build_loc(lex->pos)),
                                                  lex->quick_build_loc(op_loc));
        } catch (std::exception&) {
            move_to_next_stmt();
            return nullptr;
        }
    }
}

int AST::is_variable(const std::string& name) {
    for (const auto& [symbol_table, layer_type] : std::ranges::reverse_view(scoped_symbol_table_layer)) {
        if (const int type = is_variable(symbol_table, name); type != 0) return type;
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

void AST::move_to_next_stmt() {
    while (token != TOKEN_EOF && !is_end_of_stmt(token)) {
        this->token = lex->get_token();
    }
}
