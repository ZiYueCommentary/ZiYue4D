#include "AST.h"

const std::unordered_map<int, int> op_precedence = {
    {'=', 10},{'+', 20},{'-', 20},{'*', 30},{'/', 30}
};

void AST::parse()
{
    global_symbols.insert({ "__main", SYMBOL_TYPE_FUNCTION });
    auto signature = std::make_unique<FunctionSignatureAST>("__main", SYMBOL_TYPE_INT);
    auto function = std::make_unique<FunctionAST>(std::move(signature));
    while (true) {
        //try {
        this->token = lex->get_token();
        if (token == TOKEN_EOF) break;
        if (token == TOKEN_END_OF_STMT) continue;
        if (token == TOKEN_FUNCTION) {
            parse_function_definition();
            continue;
        }
        if (token == TOKEN_EXTERN) {
            auto function = parse_function_signature();
            extern_function_table.emplace(function->name, std::move(function));
            continue;
        }
        std::unique_ptr<ExprAST> lhs = std::move(parse_primary_expression(global_symbols));
        function->body.push_back(std::move(parse_expression(std::move(lhs), global_symbols)));
        //}
        //catch (ast_exception e) {
        //    std::cerr << e.what() << '\n';
        //    while (token != TOKEN_EOF && token != TOKEN_END_OF_STMT) { this->token = lex->get_token(); }
        //}
    }
    function_table.emplace("__main", std::move(function));
}

std::unique_ptr<ExprAST> AST::parse_primary_expression(SymbolTable& symbol_table, bool function_first)
{
    std::unique_ptr<ExprAST> lhs = nullptr;
    switch (token) {
    case TOKEN_IDENTIFIER:
    {
        std::string identifier = std::move(lex->identifier);
        token = lex->get_token();
        switch (token) {
        case TOKEN_TYPE_FLOAT:
            if (!is_variable(symbol_table, identifier)) {
                symbol_table.insert({ identifier, SYMBOL_TYPE_FLOAT });
            }
            token = lex->get_token();
            break;
        case TOKEN_TYPE_STRING:
            if (!is_variable(symbol_table, identifier)) {
                symbol_table.insert({ identifier, SYMBOL_TYPE_STRING });
            }
            token = lex->get_token();
            break;
        case TOKEN_TYPE_INT:
            if (!is_variable(symbol_table, identifier)) {
                symbol_table.insert({ identifier, SYMBOL_TYPE_INT });
            }
            token = lex->get_token();
        default:
            if (token == '=') {
                if (!is_variable(symbol_table, identifier)) {
                    symbol_table.insert({ identifier, SYMBOL_TYPE_INT });
                }
                break;
            }
            if (token == '(' || function_first) { // must be function call
                lhs = parse_call_expression(std::move(identifier), symbol_table);
                token = lex->get_token();
                return lhs;
            }
        }
        
        //if (!op_precedence.contains(token) && token != '(' && token != ')') throw ast_exception("unknown operator");
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
        lhs = std::move(parse_expression(std::move(parse_primary_expression(symbol_table, function_first)), symbol_table));
        if (token != ')') throw ast_exception("expecting closing parenthesis");
        token = lex->get_token();
        break;
    case '-':
        token = lex->get_token();
        lhs = std::make_unique<UnaryExprAST>('-', std::move(parse_primary_expression(symbol_table, function_first)));
        break;
    case TOKEN_LOGIC_NOT:
        token = lex->get_token();
        lhs = std::make_unique<UnaryExprAST>(TOKEN_LOGIC_NOT, std::move(parse_primary_expression(symbol_table, function_first)));
        break;
    default:
        throw ast_exception("expecting primary expression");
    }
    return lhs;
}

std::unique_ptr<FunctionSignatureAST> AST::parse_function_signature() {
    this->token = lex->get_token();
    if (token != TOKEN_IDENTIFIER) throw ast_exception("expecting function name");
    std::string name = std::move(lex->identifier);
    this->token = lex->get_token();
    SymbolType return_value_type = SYMBOL_TYPE_INT;
    if (token == TOKEN_TYPE_INT || token == TOKEN_TYPE_FLOAT || token == TOKEN_TYPE_STRING) {
        return_value_type = token_to_type((Token)token);
        this->token = lex->get_token();
    }
    if (token != '(') throw ast_exception("expecting opening parenthesis");
    auto function = std::make_unique<FunctionSignatureAST>(name, return_value_type);
    do {
        this->token = lex->get_token();
        if (token == ')') break;
        if (token != TOKEN_IDENTIFIER) throw ast_exception("expecting argument name");
        std::string arg_name = std::move(lex->identifier);
        this->token = lex->get_token();
        SymbolType type = SYMBOL_TYPE_INT;
        if (token == TOKEN_TYPE_INT || token == TOKEN_TYPE_FLOAT || token == TOKEN_TYPE_STRING) {
            type = token_to_type((Token)token);
            this->token = lex->get_token();
        }
        std::unique_ptr<ExprAST> default_value = nullptr;
        if (token == '=') {
            this->token = lex->get_token();
            default_value = parse_expression(parse_primary_expression(function->symbol_table, false), function->symbol_table);
        }
        function->arguments.push_back(std::make_unique<FunctionArgument>(std::move(arg_name), type, std::move(default_value)));
    } while (token == ',');
    if (token != ')') throw ast_exception("expecting closing parenthesis");
    global_symbols.insert({ name, SYMBOL_TYPE_FUNCTION });
    return function;
}

void AST::parse_function_definition() {
    auto function = std::make_unique<FunctionAST>(std::move(parse_function_signature()));
    this->token = lex->get_token();
    do {
        if (token == TOKEN_EOF) throw ast_exception("expecting end function");
        if (token == TOKEN_FUNCTION) throw ast_exception("cannot define function in function");
        if (token == TOKEN_EXTERN) throw ast_exception("cannot define extern function in function");
        if (token == TOKEN_END && (this->token = lex->get_token()) == TOKEN_FUNCTION) { break; }
        if (token == TOKEN_END_OF_STMT) { this->token = lex->get_token(); continue; }
        std::unique_ptr<ExprAST> lhs = std::move(parse_primary_expression(function->signature->symbol_table));
        function->body.push_back(std::move(parse_expression(std::move(lhs), function->signature->symbol_table)));
    } while (true);
    function_table.emplace(function->signature->name, std::move(function));
}

std::unique_ptr<CallExprAST> AST::parse_call_expression(std::string callee, SymbolTable& symbol_table) {
    std::vector<std::unique_ptr<ExprAST>> arguments = {};
    if (token == ')') return std::make_unique<CallExprAST>(std::move(callee), std::move(arguments));
    do {
        if (token == ',' || token == '(') this->token = lex->get_token();
        if (token == ')' || token == TOKEN_EOF || token == TOKEN_END_OF_STMT) { this->token = lex->get_token(); break; }
        std::unique_ptr<ExprAST> lhs = std::move(parse_primary_expression(symbol_table, false));
        arguments.push_back(std::move(parse_expression(std::move(lhs), symbol_table)));
    } while (token == ',');
    return std::make_unique<CallExprAST>(std::move(callee), std::move(arguments));
}

std::unique_ptr<ExprAST> AST::parse_expression(std::unique_ptr<ExprAST> lhs, SymbolTable& symbol_table)
{
    while (true) {
        int op = token;
        if (token == TOKEN_EOF || token == TOKEN_END_OF_STMT || token == ')' || token == ',') return lhs;

        token = lex->get_token();
        std::unique_ptr<ExprAST> rhs = std::move(parse_primary_expression(symbol_table));

        while (token != TOKEN_EOF && token != TOKEN_END_OF_STMT && token != ')' &&
            op_precedence.at(op) < op_precedence.at(token)) {
            int next_op = token;
            token = lex->get_token();
            rhs = std::make_unique<BinaryExprAST>(next_op, std::move(rhs), parse_expression(std::move(parse_primary_expression(symbol_table)), symbol_table));
        }

        lhs = std::make_unique<BinaryExprAST>(op, std::move(lhs), std::move(rhs));
    }
}

bool AST::is_variable(SymbolTable& symbol_table, const std::string& name) {
    if (symbol_table.contains(name)) {
        auto range = symbol_table.equal_range(lex->identifier);
        for (auto it = range.first; it != range.second; ++it) {
            if (is_variable_type(it->second)) return true;
        }
    }
    if (!global_symbols.contains(name)) return false;
    auto range = global_symbols.equal_range(lex->identifier);
    for (auto it = range.first; it != range.second; ++it) {
        if (is_variable_type(it->second)) return true;
    }
    return false;
}

bool AST::is_function(const std::string& name) {
    if (!global_symbols.contains(name)) return false;
    auto range = global_symbols.equal_range(lex->identifier);
    for (auto it = range.first; it != range.second; ++it) {
        if (it->second == SYMBOL_TYPE_FUNCTION) return true;
    }
    return false;
}