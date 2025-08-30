#include "AST.h"

const std::unordered_map<int, int> op_precedence = {
    {'=', 10},{'+', 20},{'-', 20},{'*', 30},{'/', 30}
};

std::vector<std::unique_ptr<ExprAST>> AST::parse()
{
    std::vector<std::unique_ptr<ExprAST>> expressions = {};
    while (true) {
        //try {
        this->token = lex->get_token();
        if (token == TOKEN_EOF) break;
        if (token == TOKEN_END_OF_STMT) continue;
        if (token == TOKEN_FUNCTION) {
            parse_function_definition();
            continue;
        }
        std::unique_ptr<ExprAST> lhs = std::move(parse_primary_expression(global_symbols));
        expressions.push_back(std::move(parse_expression(std::move(lhs), global_symbols)));
        //}
        //catch (ast_exception e) {
        //    std::cerr << e.what() << '\n';
        //    while (token != TOKEN_EOF && token != TOKEN_END_OF_STMT) { this->token = lex->get_token(); }
        //}
    }
    return expressions;
}

std::unique_ptr<ExprAST> AST::parse_primary_expression(SymbolTable& symbol_table)
{
    std::unique_ptr<ExprAST> lhs = nullptr;
    switch (token) {
    case TOKEN_IDENTIFIER:
        if (!is_variable(symbol_table, lex->identifier) && !is_function(lex->identifier)) {
            token = lex->get_token();
            switch (token) {
            case TOKEN_TYPE_FLOAT:
                symbol_table.insert({ lex->identifier, SYMBOL_TYPE_FLOAT });
                token = lex->get_token();
                break;
            case TOKEN_TYPE_STRING:
                symbol_table.insert({ lex->identifier, SYMBOL_TYPE_STRING });
                token = lex->get_token();
                break;
            case TOKEN_TYPE_INT:
                token = lex->get_token();
            default:
                symbol_table.insert({ lex->identifier, SYMBOL_TYPE_INT });
            }
        }
        else if (is_function(lex->identifier)) {
            //todo
        }
        else {
            token = lex->get_token();
            if (token == TOKEN_TYPE_INT || token == TOKEN_TYPE_FLOAT || token == TOKEN_TYPE_STRING) throw ast_exception("found redefine");
        }
        if (!op_precedence.contains(token) && token != '(' && token != ')') throw ast_exception("unknown operator");
        lhs = std::make_unique<VariableExprAST>(std::move(lex->identifier));
        break;
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
        lhs = std::move(parse_expression(std::move(parse_primary_expression(symbol_table)), symbol_table));
        if (token != ')') throw ast_exception("expecting closing parenthesis");
        token = lex->get_token();
        break;
    case '-':
        token = lex->get_token();
        lhs = std::make_unique<UnaryExprAST>('-', std::move(parse_primary_expression(symbol_table)));
        break;
    case TOKEN_LOGIC_NOT:
        token = lex->get_token();
        lhs = std::make_unique<UnaryExprAST>(TOKEN_LOGIC_NOT, std::move(parse_primary_expression(symbol_table)));
        break;
    default:
        throw ast_exception("expecting primary expression");
    }
    return lhs;
}

void AST::parse_function_definition() {
    this->token = lex->get_token();
    if (token != TOKEN_IDENTIFIER) throw ast_exception("expecting function name");
    std::string name = std::move(lex->identifier);
    this->token = lex->get_token();
    Token return_value_type = TOKEN_TYPE_INT;
    if (token == TOKEN_TYPE_INT || token == TOKEN_TYPE_FLOAT || token == TOKEN_TYPE_STRING) {
        return_value_type = (Token)token;
        this->token = lex->get_token();
    }
    if (token != '(') throw ast_exception("expecting opening parenthesis");
    auto function = std::make_unique<FunctionAST>(name, return_value_type);
    do {
        this->token = lex->get_token();
        if (token == ')') break;
        if (token != TOKEN_IDENTIFIER) throw ast_exception("expecting argument name");
        std::string arg_name = std::move(lex->identifier);
        this->token = lex->get_token();
        Token type = TOKEN_TYPE_INT;
        if (token == TOKEN_TYPE_INT || token == TOKEN_TYPE_FLOAT || token == TOKEN_TYPE_STRING) {
            type = (Token)token;
            this->token = lex->get_token();
        }
        std::unique_ptr<ExprAST> default_value = nullptr;
        if (token == '=') {
            this->token = lex->get_token();
            default_value = parse_expression(parse_primary_expression(function->symbol_table), function->symbol_table);
        }
        function->arguments.push_back(std::make_unique<FunctionArgument>(std::move(arg_name), type, std::move(default_value)));
    } while (token == ',');
    if (token != ')') throw ast_exception("expecting closing parenthesis");
    this->token = lex->get_token();
    do {
        if (token == TOKEN_EOF) throw ast_exception("expecting end function");
        if (token == TOKEN_FUNCTION) throw ast_exception("cannot define function in function");
        if (token == TOKEN_END && (this->token = lex->get_token()) == TOKEN_FUNCTION) { break; }
        if (token == TOKEN_END_OF_STMT) { this->token = lex->get_token(); continue; }
        std::unique_ptr<ExprAST> lhs = std::move(parse_primary_expression(function->symbol_table));
        function->body.push_back(std::move(parse_expression(std::move(lhs), function->symbol_table)));
    } while (true);
    global_symbols.insert({ name, SYMBOL_TYPE_FUNCTION });
    FunctionSignature signature = std::make_tuple(return_value_type, function->arguments.size());
    function_table.emplace(name, std::make_tuple(signature, std::move(function)));
    this->token = lex->get_token();
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
    if (global_symbols.contains(name)) {
        auto range = global_symbols.equal_range(lex->identifier);
        for (auto it = range.first; it != range.second; ++it) {
            if (it->second == SYMBOL_TYPE_INT || it->second == SYMBOL_TYPE_FLOAT || it->second == SYMBOL_TYPE_STRING) return true;
        }
    }
    if (!symbol_table.contains(name)) return false;
    auto range = symbol_table.equal_range(lex->identifier);
    for (auto it = range.first; it != range.second; ++it) {
        if (it->second == SYMBOL_TYPE_INT || it->second == SYMBOL_TYPE_FLOAT || it->second == SYMBOL_TYPE_STRING) return true;
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