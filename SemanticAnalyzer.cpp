#include "SemanticAnalyzer.h"

#include <algorithm>
#include <iostream>
#include <ranges>

bool SemanticAnalyzer::analyze() {
    scope_function = &ast->function_table.equal_range("main").first->second->signature;
    bool occur_errors = false;
    scoped_symbol_types.emplace_back();
    for (auto& [name, value] : ast->constant_table) {
        scoped_symbol_types.back().emplace(name, static_cast<SymbolType>(ast->is_variable(name)));
        if (!is_constant_expression(value)) {
            ast->lex->source_mgr.PrintMessage(value->range.Start, llvm::SourceMgr::DK_Error,
                                              "expression must be constant", value->range);
            occur_errors = true;
        }
        const SymbolType value_type = get_type(value);
        if (const int constant_type = ast->is_variable(name); value_type != constant_type) {
            ast->lex->source_mgr.PrintMessage(value->range.Start, llvm::SourceMgr::DK_Error,
                                              symbol_type_to_literal(value_type) + " is not " +
                                              symbol_type_to_literal(static_cast<SymbolType>(constant_type)),
                                              value->range);
            occur_errors = true;
        }
    }
    for (auto& [name, value] : ast->global_table) {
        scoped_symbol_types.back().emplace(name, static_cast<SymbolType>(ast->is_variable(name)));
        const SymbolType value_type = get_type(value);
        if (const int variable_type = ast->is_variable(name); value_type != variable_type) {
            ast->lex->source_mgr.PrintMessage(value->range.Start, llvm::SourceMgr::DK_Error,
                                              symbol_type_to_literal(value_type) + " cannot convert to " +
                                              symbol_type_to_literal(static_cast<SymbolType>(variable_type)),
                                              value->range);
            occur_errors = true;
        }
    }
    for (auto& function : ast->function_table | std::views::values) {
        scope_function = &function->signature;
        scoped_symbol_types.emplace_back();
        ast->scoped_symbol_table_layer.emplace_back(&function->signature->symbol_table, SYMBOL_TABLE_TYPE_FUNCTION);
        const auto& symbol_table = function->signature->symbol_table;
        for (const auto& [name, type] : symbol_table) {
            if (is_variable_type(type)) scoped_symbol_types.back().emplace(name, type);
        }

        for (const auto& arg : function->signature->arguments) {
            const SymbolType value_type = get_type(arg->default_value);
            if (const int argument_type = arg->type; value_type != argument_type) {
                ast->lex->source_mgr.PrintMessage(arg->default_value->range.Start, llvm::SourceMgr::DK_Error,
                                                  symbol_type_to_literal(value_type) + " cannot convert to " +
                                                  symbol_type_to_literal(static_cast<SymbolType>(argument_type)),
                                                  arg->default_value->range);
                occur_errors = true;
            }
        }
        for (auto& expr : function->body) {
            try {
                get_type(expr);
            } catch (std::exception&) {
                occur_errors = true;
            }
        }
        ast->scoped_symbol_table_layer.pop_back();
        scoped_symbol_types.pop_back();
    }
    return occur_errors;
}

bool SemanticAnalyzer::can_convert_to(const std::unique_ptr<ExprAST>& old_expr, const SymbolType new_type) {
    const SymbolType old_type = get_type(old_expr);
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
                ast->lex->source_mgr.PrintMessage(old_expr->range.Start, llvm::SourceMgr::DK_Warning,
                                                  "float to int may cause precision loss", old_expr->range);
                return true;
            }
            return new_type == SYMBOL_TYPE_STRING;
        default:
            return false;
    }
}

const std::unique_ptr<FunctionSignatureAST>* SemanticAnalyzer::seek_best_match_function(const CallExprAST& expr) {
    auto [begin, end] = ast->function_table.equal_range(expr.name);
    const std::unique_ptr<FunctionSignatureAST>* current_candidate = nullptr;
    size_t current_mandatory = 0;
    for (auto& it = begin; it != end; ++it) {
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
        const auto& call = dynamic_cast<UnaryExprAST&>(*expr);
        switch (call.op) {
            case '&': {
                const auto& ident = dynamic_cast<VariableExprAST&>(*call.expr);
                // TODO variable
                if (!ast->scoped_symbol_table_layer.front().symbol_table->contains(ident.name)) {
                    ast->lex->source_mgr.PrintMessage(ident.range.Start, llvm::SourceMgr::DK_Error,
                                                      "unknown function name", ident.range);
                    throw std::exception();
                }
                const auto candidates = ast->function_table.equal_range(ident.name);
                if (std::distance(candidates.first, candidates.second) > 1)
                    ast->lex->source_mgr.PrintMessage(call.range.Start, llvm::SourceMgr::DK_Warning,
                                                      "retrieving function pointer which has overloading", call.range);
                return SYMBOL_TYPE_POINTER;
            }
            case '-':
            case TOKEN_LOGIC_NOT: {
                const SymbolType type = get_type(call.expr);
                if (type == SYMBOL_TYPE_STRING) {
                    ast->lex->source_mgr.PrintMessage(call.range.Start, llvm::SourceMgr::DK_Error,
                                                      "unary operator cannot apply to the expression", call.range);
                    throw std::exception();
                }
                return call.op == TOKEN_LOGIC_NOT ? SYMBOL_TYPE_INT : type;
            }
            default:
                ast->lex->source_mgr.PrintMessage(call.range.Start, llvm::SourceMgr::DK_Error,
                                                  "unknown unary operator", call.range);
                throw std::exception();
        }
    }
    if (typeid(*expr) == typeid(CallExprAST)) {
        const auto& call = dynamic_cast<CallExprAST&>(*expr);
        const auto candidate = seek_best_match_function(call);
        if (candidate != nullptr) {
            for (int i = 0; i < call.arguments.size(); i++) {
                const SymbolType new_type = (*candidate)->arguments.at(i)->type;
                if (!can_convert_to(call.arguments.at(i), new_type)) {
                    ast->lex->source_mgr.PrintMessage(call.arguments.at(i)->range.Start, llvm::SourceMgr::DK_Error,
                                                      "cannot convert " + symbol_type_to_literal(
                                                          get_type(call.arguments.at(i))) + " to " +
                                                      symbol_type_to_literal(new_type), call.arguments.at(i)->range);
                    throw std::exception();
                }
            }
            return (*candidate)->return_value_type;
        }
        ast->lex->source_mgr.PrintMessage(call.range.Start, llvm::SourceMgr::DK_Error,
                                          "no function that matches the requirement", call.range);
        throw std::exception();
    }
    if (typeid(*expr) == typeid(BinaryExprAST)) {
        auto& bi_expr = dynamic_cast<BinaryExprAST&>(*expr);
        const SymbolType lhs_type = get_type(bi_expr.lhs);
        const SymbolType rhs_type = get_type(bi_expr.rhs);
        if (bi_expr.op == '=') {
            if (typeid(*bi_expr.lhs) == typeid(VariableExprAST)) {
                const auto& var = dynamic_cast<VariableExprAST&>(*bi_expr.lhs);
                if (ast->constant_table.contains(var.name)) {
                    ast->lex->source_mgr.PrintMessage(bi_expr.op_loc, llvm::SourceMgr::DK_Error,
                                                      "cannot realloc constant value", bi_expr.range);
                    throw std::exception();
                }
            }
            if (rhs_type != SYMBOL_TYPE_VOID) {
                switch (lhs_type) {
                    case SYMBOL_TYPE_INT:
                        if (rhs_type == SYMBOL_TYPE_POINTER) {
                            ast->lex->source_mgr.PrintMessage(bi_expr.op_loc, llvm::SourceMgr::DK_Warning,
                                                              "assigning pointer to a integer variable is deprecated",
                                                              bi_expr.range, {
                                                                  llvm::SMFixIt(
                                                                      bi_expr.op_loc, "assigning to a pointer variable")
                                                              });
                            return SYMBOL_TYPE_POINTER;
                        }
                        if (rhs_type == SYMBOL_TYPE_FLOAT) {
                            ast->lex->source_mgr.PrintMessage(bi_expr.op_loc, llvm::SourceMgr::DK_Warning,
                                                              "float to int may cause precision loss", bi_expr.range);
                        }
                    case SYMBOL_TYPE_FLOAT:
                        if (rhs_type != SYMBOL_TYPE_STRING) return lhs_type;
                        break;
                    case SYMBOL_TYPE_STRING:
                        if (rhs_type == SYMBOL_TYPE_POINTER) {
                            ast->lex->source_mgr.PrintMessage(bi_expr.op_loc, llvm::SourceMgr::DK_Warning,
                                                              "assigning a pointer to a string variable, please use @ for pointer type instead",
                                                              bi_expr.range);
                        }
                        return lhs_type;
                    case SYMBOL_TYPE_POINTER:
                        if (rhs_type == SYMBOL_TYPE_POINTER || rhs_type == SYMBOL_TYPE_STRING) {
                            if (rhs_type == SYMBOL_TYPE_STRING)
                                ast->lex->source_mgr.PrintMessage(bi_expr.op_loc, llvm::SourceMgr::DK_Warning,
                                                                  "assigning a string to a pointer variable. lifecycle of string is managed by ZiYue4D, the pointer may be a wild pointer",
                                                                  bi_expr.range);
                            return lhs_type;
                        }
                }
            }
            ast->lex->source_mgr.PrintMessage(bi_expr.op_loc, llvm::SourceMgr::DK_Error,
                                              "cannot convert " + symbol_type_to_literal(lhs_type) + " to " +
                                              symbol_type_to_literal(rhs_type), bi_expr.range);
            throw std::exception();
        }
        if (lhs_type == SYMBOL_TYPE_STRING || rhs_type == SYMBOL_TYPE_STRING) {
            if (bi_expr.op == '+') return SYMBOL_TYPE_STRING;
            if (bi_expr.op == TOKEN_EQUALS) return SYMBOL_TYPE_INT;
            ast->lex->source_mgr.PrintMessage(bi_expr.op_loc, llvm::SourceMgr::DK_Error,
                                              "invalid operator for string", bi_expr.range);
            throw std::exception();
        }
        if (lhs_type == SYMBOL_TYPE_FLOAT || rhs_type == SYMBOL_TYPE_FLOAT) {
            if (is_relational_operator(bi_expr.op)) return SYMBOL_TYPE_INT;
            if (is_bitwise_or_logic_operator(bi_expr.op)) {
                ast->lex->source_mgr.PrintMessage(bi_expr.op_loc, llvm::SourceMgr::DK_Warning,
                                                  "float to int may cause precision loss", bi_expr.range);
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
        ast->lex->source_mgr.PrintMessage(var.range.Start, llvm::SourceMgr::DK_Error,
                                          "unknown variable " + var.name, var.range);
        throw std::exception();
    }
    if (typeid(*expr) == typeid(ReturnExprAST)) {
        const auto& ret = dynamic_cast<ReturnExprAST&>(*expr);
        if (ret.expr == nullptr) return (*scope_function)->return_value_type;
        const SymbolType type = get_type(ret.expr);
        if (!can_convert_to(ret.expr, (*scope_function)->return_value_type)) {
            ast->lex->source_mgr.PrintMessage(ret.expr->range.Start, llvm::SourceMgr::DK_Error,
                                              "cannot convert " + symbol_type_to_literal(type) + " to " +
                                              symbol_type_to_literal((*scope_function)->return_value_type),
                                              ret.expr->range);
            throw std::exception();
        }
        return type;
    }
    if (typeid(*expr) == typeid(IfStatementAST)) {
        auto& if_statement = dynamic_cast<IfStatementAST&>(*expr);
        if (!can_convert_to(if_statement.condition, SYMBOL_TYPE_INT)) {
            ast->lex->source_mgr.PrintMessage(if_statement.condition->range.Start, llvm::SourceMgr::DK_Error,
                                              "if condition statement must be integer", if_statement.condition->range);
            throw std::exception();
        }
        // statement true
        {
            scoped_symbol_types.emplace_back();
            auto& symbol_table = if_statement.statement_true_symbol_table;
            ast->scoped_symbol_table_layer.emplace_back(&symbol_table, SYMBOL_TABLE_TYPE_BRANCH);
            for (const auto& [name, type] : symbol_table) {
                if (is_variable_type(type)) scoped_symbol_types.back().emplace(name, type);
            }
            for (auto& true_expr : if_statement.statement_true) {
                get_type(true_expr);
            }
            ast->scoped_symbol_table_layer.pop_back();
            scoped_symbol_types.pop_back();
        }
        // statement false
        {
            scoped_symbol_types.emplace_back();
            auto& symbol_table = if_statement.statement_false_symbol_table;
            ast->scoped_symbol_table_layer.emplace_back(&symbol_table, SYMBOL_TABLE_TYPE_BRANCH);
            for (
                const auto& [name, type] : symbol_table) {
                if (is_variable_type(type)) scoped_symbol_types.back().emplace(name, type);
            }
            for (auto& false_expr : if_statement.statement_false) {
                get_type(false_expr);
            }
            ast->scoped_symbol_table_layer.pop_back();
            scoped_symbol_types.pop_back();
        }
        return SYMBOL_TYPE_VOID;
    }
    if (typeid(*expr) == typeid(WhileStatementAST)) {
        auto& while_statement = dynamic_cast<WhileStatementAST&>(*expr);
        if (!can_convert_to(while_statement.condition, SYMBOL_TYPE_INT)) {
            ast->lex->source_mgr.PrintMessage(while_statement.condition->range.Start, llvm::SourceMgr::DK_Error,
                                              "while condition statement must be integer",
                                              while_statement.condition->range);
            throw std::exception();
        }
        scoped_symbol_types.emplace_back();
        auto& symbol_table = while_statement.statement_true_symbol_table;
        ast->scoped_symbol_table_layer.emplace_back(&symbol_table, SYMBOL_TABLE_TYPE_LOOP);
        for (const auto& [name, type] : symbol_table) {
            if (is_variable_type(type)) scoped_symbol_types.back().emplace(name, type);
        }
        for (auto& true_expr : while_statement.statement_true) {
            get_type(true_expr);
        }
        ast->scoped_symbol_table_layer.pop_back();
        scoped_symbol_types.pop_back();
        return SYMBOL_TYPE_VOID;
    }
    if (typeid(*expr) == typeid(ExitAST) || typeid(*expr) == typeid(ContinueAST)) return SYMBOL_TYPE_VOID;
    ast->lex->source_mgr.PrintMessage(expr->range.Start, llvm::SourceMgr::DK_Error,
                                      "unknown expression! is this a compiler bug?", expr->range);
    throw std::exception();
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
    return token == TOKEN_NOT_EQUALS || token == TOKEN_EQUALS || token == TOKEN_LESS_THAN ||
           token == TOKEN_LESS_THAN_OR_EQUALS || token == TOKEN_GREATER_THAN || token == TOKEN_GREATER_THAN_OR_EQUALS;
}

bool SemanticAnalyzer::is_bitwise_or_logic_operator(int token) {
    return token == '&' || token == TOKEN_BITWISE_OR || token == TOKEN_LOGIC_OR ||
           token == TOKEN_LOGIC_AND || token == TOKEN_LOGIC_NOT;
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
