#include "SemanticAnalyzer.h"
#include <iostream>

void SemanticAnalyzer::analyze()
{
    for (auto& function : ast->function_table) {
        for (auto& arg : function.second->signature->arguments) {
            try {
                if (arg->default_value != nullptr && !can_convert_to(get_type(arg->default_value), arg->type)) {
                    std::cerr << "mismatch argument value at " << function.second->signature->name << ": " << arg->name << " is " << arg->type << '\n';
                }
            }
            catch (semantic_exception e) {
                std::cerr << "invalid syntax at " << function.second->to_string() << " signature: " << e.what() << '\n';
            }
        }
        scope_symbol_table = &function.second->signature->symbol_table;
        for (auto& expr : function.second->body) {
            try {
                get_type(expr);
            }
            catch (semantic_exception e) {
                std::cerr << "invalid syntax at " << function.second->to_string() << " definition: " << e.what() << '\n';
            }
        }
    }
}

bool SemanticAnalyzer::can_convert_to(SymbolType old_type, SymbolType new_type) {
    switch (old_type) {
    case SYMBOL_TYPE_INT:
        return new_type == SYMBOL_TYPE_FLOAT || new_type == SYMBOL_TYPE_STRING;
    case SYMBOL_TYPE_FLOAT:
        if (new_type == SYMBOL_TYPE_INT) {
            throw semantic_exception("unsafe conversion: float to int may cause precision loss");
        }
        return new_type == SYMBOL_TYPE_STRING;
    default:
        return false;
    }
}

SymbolType SemanticAnalyzer::get_type(const std::unique_ptr<ExprAST>& expr)
{
    if (typeid(*expr) == typeid(FloatExprAST)) {
        return SYMBOL_TYPE_FLOAT;
    }
    if (typeid(*expr) == typeid(IntegerExprAST)) {
        return SYMBOL_TYPE_INT;
    }
    if (typeid(*expr) == typeid(StringExprAST)) {
        return SYMBOL_TYPE_STRING;
    }
    if (typeid(*expr) == typeid(CallExprAST)) {
        auto& call = dynamic_cast<CallExprAST&>(*expr);
        auto candidates = ast->function_table.equal_range(call.name);
        for (auto it = candidates.first; it != candidates.second; ++it) {
            if (it->second->signature->arguments.size() >= call.arguments.size()) {
                return it->second->signature->return_value_type;
            }
        }
        if (ast->extern_function_table.contains(call.name)) {
            auto& candidates = ast->extern_function_table.at(call.name);
            if (candidates->arguments.size() == call.arguments.size()) return candidates->return_value_type;
        }
        throw semantic_exception("no function that matches the requirement");
    }
    if (typeid(*expr) == typeid(UnaryExprAST)) {
        auto& call = dynamic_cast<UnaryExprAST&>(*expr);
        switch (call.op) {
        case '-':
        case TOKEN_LOGIC_NOT:
        {
            SymbolType type = get_type(call.expr);
            if (type == SYMBOL_TYPE_STRING) {
                throw semantic_exception("unary operator cannot apply to the expression");
            }
            return type;
        }
        default:
            throw semantic_exception("invalid unary operator");
        }
    }
    if (typeid(*expr) == typeid(BinaryExprAST)) {
        auto& biexpr = dynamic_cast<BinaryExprAST&>(*expr);
        SymbolType lhs_type = get_type(biexpr.lhs);
        SymbolType rhs_type = get_type(biexpr.rhs);
        if (biexpr.op == '=') {
            switch (lhs_type)
            {
            case SYMBOL_TYPE_INT:
            case SYMBOL_TYPE_FLOAT:
                if (rhs_type != SYMBOL_TYPE_STRING) return lhs_type;
                break;
            case SYMBOL_TYPE_STRING:
                return lhs_type;
            }
            throw semantic_exception("bad conversion");
        }
        if (lhs_type == SYMBOL_TYPE_STRING || rhs_type == SYMBOL_TYPE_STRING) return SYMBOL_TYPE_STRING;
        if (lhs_type == SYMBOL_TYPE_FLOAT || rhs_type == SYMBOL_TYPE_FLOAT) return SYMBOL_TYPE_FLOAT;
        return SYMBOL_TYPE_INT;
    }
    if (typeid(*expr) == typeid(VariableExprAST)) {
        auto& var = dynamic_cast<VariableExprAST&>(*expr);
        if (scope_symbol_table->contains(var.name)) {
            auto range = scope_symbol_table->equal_range(var.name);
            for (auto it = range.first; it != range.second; ++it) {
                if (is_variable_type(it->second)) return it->second;
            }
        }
        if (!ast->global_symbols.contains(var.name)) {
            throw semantic_exception("unknown variable");
        }
        auto range = ast->global_symbols.equal_range(var.name);
        for (auto it = range.first; it != range.second; ++it) {
            if (is_variable_type(it->second)) return it->second;
        }
    }
    throw semantic_exception("unknown expression");
}