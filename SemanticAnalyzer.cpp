#include "SemanticAnalyzer.h"

#include <algorithm>
#include <iostream>
#include <ranges>
#include "termcolor.hpp"

bool SemanticAnalyzer::analyze() {
    scope_function = &ast->function_table.equal_range("main").first->second->signature;
    bool occur_errors = false;
    scoped_symbol_types.emplace_back();
    for (auto& [name, value] : ast->constant_table) {
        scoped_symbol_types.back().emplace(name, static_cast<SymbolType>(ast->is_variable(name)));
        if (!is_constant_expression(value)) {
            std::cerr << termcolor::red << "expression must be constant\n" << termcolor::reset;
            occur_errors = true;
        }
        try {
            if (get_type(value) != ast->is_variable(name)) {
                std::cerr << termcolor::red << "mismatch type at constant " << name << " definition\n" <<
                        termcolor::reset;
                occur_errors = true;
            }
        } catch (semantic_exception& e) {
            std::cerr << termcolor::red << "invalid syntax at constant " << name << " definition: " << e.
                    what() << '\n' << termcolor::reset;
            occur_errors = true;
        }
    }
    for (auto& [name, value] : ast->global_table) {
        scoped_symbol_types.back().emplace(name, static_cast<SymbolType>(ast->is_variable(name)));
        try {
            if (value != nullptr && get_type(value) != ast->is_variable(name)) {
                std::cerr << termcolor::red << "mismatch type at constant " << name << " definition\n" <<
                        termcolor::reset;
                occur_errors = true;
            }
        } catch (semantic_exception& e) {
            std::cerr << termcolor::red << "invalid syntax at constant " << name << " definition: " << e.
                    what() << '\n' << termcolor::reset;
            occur_errors = true;
        }
    }
    for (auto& function : ast->function_table | std::views::values) {
        scope_function = &function->signature;
        scoped_symbol_types.emplace_back();
        ast->scoped_symbol_table.emplace_back(&function->signature->symbol_table);
        const auto& symbol_table = function->signature->symbol_table;
        for (const auto& [name, type] : symbol_table) {
            if (is_variable_type(type)) scoped_symbol_types.back().emplace(name, type);
        }

        for (const auto& arg : function->signature->arguments) {
            try {
                if (arg->default_value != nullptr && !can_convert_to(get_type(arg->default_value), arg->type)) {
                    std::cerr << termcolor::red << "mismatch argument default value at " << function->signature->name <<
                            ": " <<
                            arg->name << " is " << arg->type << '\n' << termcolor::reset;
                }
            } catch (semantic_exception& e) {
                std::cerr << termcolor::red << "invalid syntax at " << readable_function_signature(function) <<
                        " signature: " << e
                        .what() << '\n' << termcolor::reset;
                occur_errors = true;
            }
        }
        for (auto& expr : function->body) {
            try {
                get_type(expr);
            } catch (semantic_exception& e) {
                std::cerr << termcolor::red << "invalid syntax at " << readable_function_signature(function) <<
                        " definition: " <<
                        e.what() << '\n' << termcolor::reset;
                occur_errors = true;
            }
        }
        ast->scoped_symbol_table.pop_back();
        scoped_symbol_types.pop_back();
    }
    return occur_errors;
}

bool SemanticAnalyzer::can_convert_to(SymbolType old_type, SymbolType new_type) {
    if (old_type == new_type) return true;
    switch (old_type) {
        case SYMBOL_TYPE_VOID:
            return false;
        case SYMBOL_TYPE_STRING:
            return new_type == SYMBOL_TYPE_STRING || new_type == SYMBOL_TYPE_POINTER;
        case SYMBOL_TYPE_INT:
            return new_type == SYMBOL_TYPE_FLOAT || new_type == SYMBOL_TYPE_STRING;
        case SYMBOL_TYPE_FLOAT:
            if (new_type == SYMBOL_TYPE_INT) {
                std::cerr << termcolor::yellow << "unsafe conversion: float to int may cause precision loss\n" <<
                        termcolor::reset;
                return true;
            }
            return new_type == SYMBOL_TYPE_STRING;
        default:
            return false;
    }
}

const std::unique_ptr<FunctionSignatureAST>* SemanticAnalyzer::seek_best_match_function(const CallExprAST& expr) {
    auto candidates = ast->function_table.equal_range(expr.name);
    std::unique_ptr<FunctionSignatureAST>* current_candidate = nullptr;
    size_t current_mandatory = 0;
    for (auto& it = candidates.first; it != candidates.second; ++it) {
        const size_t mandatory_args = std::ranges::count_if(it->second->signature->arguments,
                                                            [](const std::unique_ptr<FunctionArgument>& arg) {
                                                                return arg->default_value == nullptr;
                                                            });
        const size_t optional_args = it->second->signature->arguments.size() - mandatory_args;
        if (expr.arguments.size() >= mandatory_args && mandatory_args >= current_mandatory) {
            current_candidate = &it->second->signature;
            current_mandatory = mandatory_args;
        }
        if (expr.arguments.size() == mandatory_args + optional_args) return &it->second->signature; // best match
    }
    if (current_candidate == nullptr && ast->extern_function_table.contains(expr.name)) {
        const auto& candidate = ast->extern_function_table.at(expr.name);
        const size_t mandatory_args = std::ranges::count_if(candidate->arguments,
                                                            [](const std::unique_ptr<FunctionArgument>& arg) {
                                                                return arg->default_value == nullptr;
                                                            });
        if (expr.arguments.size() >= mandatory_args && expr.arguments.size() <= candidate->arguments.size()) {
            return &candidate;
        }
    }
    return current_candidate;
}

SymbolType SemanticAnalyzer::get_type(const std::unique_ptr<ExprAST>& expr) {
    if (typeid(*expr) == typeid(FloatExprAST)) {
        return SYMBOL_TYPE_FLOAT;
    }
    if (typeid(*expr) == typeid(IntegerExprAST)) {
        return SYMBOL_TYPE_INT;
    }
    if (typeid(*expr) == typeid(StringExprAST)) {
        return SYMBOL_TYPE_STRING;
    }
    if (typeid(*expr) == typeid(UnaryExprAST)) {
        auto& call = dynamic_cast<UnaryExprAST&>(*expr);
        switch (call.op) {
            case '&': {
                const auto& ident = dynamic_cast<VariableExprAST&>(*call.expr);
                // TODO variable
                if (!ast->scoped_symbol_table.front()->contains(ident.name)) throw semantic_exception(
                    "unknown identifier");
                const auto candidates = ast->function_table.equal_range(ident.name);
                if (std::distance(candidates.first, candidates.second) > 1)
                    std::cerr << termcolor::yellow <<
                            "undefined behavior: retrieving function pointer which has overloading\n" <<
                            termcolor::reset;
                return SYMBOL_TYPE_POINTER;
            }
            case '-':
            case TOKEN_LOGIC_NOT: {
                const SymbolType type = get_type(call.expr);
                if (type == SYMBOL_TYPE_STRING) {
                    throw semantic_exception("unary operator cannot apply to the expression");
                }
                return call.op == TOKEN_LOGIC_NOT ? SYMBOL_TYPE_INT : type;
            }
            default:
                throw semantic_exception("invalid unary operator");
        }
    }
    if (typeid(*expr) == typeid(CallExprAST)) {
        const auto& call = dynamic_cast<CallExprAST&>(*expr);
        const auto candidate = seek_best_match_function(call);
        if (candidate != nullptr) {
            for (int i = 0; i < call.arguments.size(); i++) {
                if (!can_convert_to(get_type(call.arguments.at(i)), (*candidate)->arguments.at(i)->type)) {
                    throw semantic_exception("mismatch argument type");
                }
            }
            return (*candidate)->return_value_type;
        }
        throw semantic_exception("no function that matches the requirement");
    }
    if (typeid(*expr) == typeid(BinaryExprAST)) {
        auto& biexpr = dynamic_cast<BinaryExprAST&>(*expr);
        const SymbolType lhs_type = get_type(biexpr.lhs);
        const SymbolType rhs_type = get_type(biexpr.rhs);
        if (biexpr.op == '=') {
            if (typeid(*biexpr.lhs) == typeid(VariableExprAST)) {
                const auto& var = dynamic_cast<VariableExprAST&>(*biexpr.lhs);
                if (ast->constant_table.contains(var.name)) {
                    throw semantic_exception("cannot realloc constant value");
                }
            }
            if (rhs_type != SYMBOL_TYPE_VOID) {
                switch (lhs_type) {
                    case SYMBOL_TYPE_INT:
                        if (rhs_type == SYMBOL_TYPE_POINTER) {
                            std::cerr << termcolor::yellow <<
                                    "deprecated: assigning pointer to a integer variable, please use * for pointer type instead."
                                    << termcolor::reset;
                            return SYMBOL_TYPE_POINTER;
                        }
                        if (rhs_type == SYMBOL_TYPE_FLOAT) {
                            std::cerr << termcolor::yellow <<
                                    "unsafe conversion: float to int may cause precision loss\n" << termcolor::reset;
                        }
                    case SYMBOL_TYPE_FLOAT:
                        if (rhs_type != SYMBOL_TYPE_STRING) return lhs_type;
                        break;
                    case SYMBOL_TYPE_STRING:
                        if (rhs_type == SYMBOL_TYPE_POINTER) {
                            std::cerr << termcolor::yellow <<
                                    "undefined behavior: assigning a pointer to a string variable, please use * for pointer type instead."
                                    << termcolor::reset;
                        }
                        return lhs_type;
                    case SYMBOL_TYPE_POINTER:
                        if (rhs_type == SYMBOL_TYPE_POINTER || rhs_type == SYMBOL_TYPE_STRING) {
                            if (rhs_type == SYMBOL_TYPE_STRING)
                                std::cerr << termcolor::yellow <<
                                        "undefined behavior: assigning a string to a pointer variable. lifecycle of string is managed by ZiYue4D, the pointer may be a wild pointer."
                                        << termcolor::reset;
                            return lhs_type;
                        }
                }
            }
            throw semantic_exception("bad conversion");
        }
        if (lhs_type == SYMBOL_TYPE_STRING || rhs_type == SYMBOL_TYPE_STRING) {
            if (biexpr.op == '+') return SYMBOL_TYPE_STRING;
            if (biexpr.op == TOKEN_EQUALS) return SYMBOL_TYPE_INT;
            throw semantic_exception("invalid operation");
        }
        if ((lhs_type == SYMBOL_TYPE_FLOAT || rhs_type == SYMBOL_TYPE_FLOAT)) {
            if (is_relational_operator(biexpr.op)) return SYMBOL_TYPE_INT;
            if (is_bitwise_or_logic_operator(biexpr.op)) {
                std::cerr << termcolor::yellow << "unsafe conversion: float to int may cause precision loss\n" <<
                        termcolor::reset;
                return SYMBOL_TYPE_INT;
            }
            return SYMBOL_TYPE_FLOAT;
        }
        return SYMBOL_TYPE_INT;
    }
    if (typeid(*expr) == typeid(VariableExprAST)) {
        const auto& var = dynamic_cast<VariableExprAST&>(*expr);
        for (const auto& scoped_symbol_table : std::ranges::reverse_view(scoped_symbol_types)) {
            if (scoped_symbol_table.contains(var.name)) return scoped_symbol_table.at(var.name);
        }
        throw semantic_exception("unknown variable");
    }
    if (typeid(*expr) == typeid(ReturnExprAST)) {
        const auto& ret = dynamic_cast<ReturnExprAST&>(*expr);
        if (ret.expr == nullptr) return (*scope_function)->return_value_type;
        const SymbolType type = get_type(ret.expr);
        if (!can_convert_to(type, (*scope_function)->return_value_type))
            throw semantic_exception("mismatched return value type");
        return type;
    }
    if (typeid(*expr) == typeid(IfStatementAST)) {
        auto& if_statement = dynamic_cast<IfStatementAST&>(*expr);
        if (!can_convert_to(get_type(if_statement.condition), SYMBOL_TYPE_INT))
            throw semantic_exception("if condition must be integer");
        // statement true
        {
            scoped_symbol_types.emplace_back();
            auto& symbol_table = if_statement.statement_true_symbol_table;
            ast->scoped_symbol_table.emplace_back(&symbol_table);
            for (const auto& [name, type] : symbol_table) {
                if (is_variable_type(type)) scoped_symbol_types.back().emplace(name, type);
            }
            for (auto& true_expr : if_statement.statement_true) {
                get_type(true_expr);
            }
            ast->scoped_symbol_table.pop_back();
            scoped_symbol_types.pop_back();
        }
        // statement false
        {
            scoped_symbol_types.emplace_back();
            auto& symbol_table = if_statement.statement_false_symbol_table;
            ast->scoped_symbol_table.emplace_back(&symbol_table);
            for (
                 const auto& [name, type] : symbol_table) {
                if (is_variable_type(type)) scoped_symbol_types.back().emplace(name, type);
            }
            for (auto& false_expr : if_statement.statement_false) {
                get_type(false_expr);
            }
            ast->scoped_symbol_table.pop_back();
            scoped_symbol_types.pop_back();
        }
        return SYMBOL_TYPE_VOID;
    }
    if (typeid(*expr) == typeid(WhileStatementAST)) {
        auto& while_statement = dynamic_cast<WhileStatementAST&>(*expr);
        if (!can_convert_to(get_type(while_statement.condition), SYMBOL_TYPE_INT))
            throw semantic_exception("while condition must be integer");
        scoped_symbol_types.emplace_back();
        auto& symbol_table = while_statement.statement_true_symbol_table;
        ast->scoped_symbol_table.emplace_back(&symbol_table);
        for (const auto& [name, type] : symbol_table) {
            if (is_variable_type(type)) scoped_symbol_types.back().emplace(name, type);
        }
        for (auto& true_expr : while_statement.statement_true) {
            get_type(true_expr);
        }
        ast->scoped_symbol_table.pop_back();
        scoped_symbol_types.pop_back();
        return SYMBOL_TYPE_VOID;
    }
    throw semantic_exception("unknown expression");
}

bool SemanticAnalyzer::is_constant_expression(const std::unique_ptr<ExprAST>& expr) {
    if (typeid(*expr) == typeid(FloatExprAST) || typeid(*expr) == typeid(IntegerExprAST) ||
        typeid(*expr) == typeid(StringExprAST)) {
        return true;
    }
    if (typeid(*expr) == typeid(UnaryExprAST)) {
        const auto& call = dynamic_cast<UnaryExprAST&>(*expr);
        switch (call.op) {
            case '&':
                return false;
            case '-':
            case TOKEN_LOGIC_NOT:
                return is_constant_expression(call.expr);
        }
    }
    if (typeid(*expr) == typeid(CallExprAST)) {
        return false;
    }
    if (typeid(*expr) == typeid(BinaryExprAST)) {
        const auto& biexpr = dynamic_cast<BinaryExprAST&>(*expr);
        return is_constant_expression(biexpr.lhs) && is_constant_expression(biexpr.rhs);
    }
    if (typeid(*expr) == typeid(VariableExprAST)) {
        const auto& var = dynamic_cast<VariableExprAST&>(*expr);
        return ast->constant_table.contains(var.name);
    }
    return false;
}

bool SemanticAnalyzer::is_relational_operator(int token) {
    return token == TOKEN_NOT_EQUALS || token == TOKEN_EQUALS || token == TOKEN_LESS_THAN || token ==
           TOKEN_LESS_THAN_OR_EQUALS || token == TOKEN_GREATER_THAN || token == TOKEN_GREATER_THAN_OR_EQUALS;
}

bool SemanticAnalyzer::is_bitwise_or_logic_operator(int token) {
    return token == TOKEN_BITWISE_AND || token == TOKEN_BITWISE_OR || token == TOKEN_LOGIC_OR || token ==
           TOKEN_LOGIC_AND || token == TOKEN_LOGIC_NOT;
}

std::string SemanticAnalyzer::readable_function_signature(const std::unique_ptr<FunctionSignatureAST>& signature) {
    std::string result = signature->name + '(';
    for (const auto& arg : signature->arguments) {
        result += arg->name;
        switch (arg->type) {
            case SYMBOL_TYPE_INT: result += '%';
                break;
            case SYMBOL_TYPE_FLOAT: result += '#';
                break;
            case SYMBOL_TYPE_STRING: result += '$';
                break;
            case SYMBOL_TYPE_POINTER: result += '@';
                break;
            default: break;
        }
        result += ',';
    }
    if (!signature->arguments.empty()) result.pop_back();
    result.push_back(')');
    return result;
}

std::string SemanticAnalyzer::readable_function_signature(const std::unique_ptr<FunctionAST>& function) {
    return std::move(readable_function_signature(function->signature));
}
