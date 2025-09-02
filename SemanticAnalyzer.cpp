#include "SemanticAnalyzer.h"
#include <iostream>
#include <algorithm>

void SemanticAnalyzer::analyze()
{
    for (auto& function : ast->function_table) {
        for (auto& arg : function.second->signature->arguments) {
            try {
                if (arg->default_value != nullptr && !can_convert_to(get_type(arg->default_value), arg->type)) {
                    std::cerr << "mismatch argument default value at " << function.second->signature->name << ": " << arg->name << " is " << arg->type << '\n';
                }
            }
            catch (semantic_exception e) {
                std::cerr << "invalid syntax at " << readable_function_signature(function.second) << " signature: " << e.what() << '\n';
            }
        }
        scope = &function.second->signature;
        for (auto& expr : function.second->body) {
            try {
                get_type(expr);
            }
            catch (semantic_exception e) {
                std::cerr << "invalid syntax at " << readable_function_signature(function.second) << " definition: " << e.what() << '\n';
            }
        }
    }
}

bool SemanticAnalyzer::can_convert_to(SymbolType old_type, SymbolType new_type) {
    if (old_type == new_type) return true;
    switch (old_type) {
    case SYMBOL_TYPE_INT:
        return new_type == SYMBOL_TYPE_FLOAT || new_type == SYMBOL_TYPE_STRING;
    case SYMBOL_TYPE_FLOAT:
        if (new_type == SYMBOL_TYPE_INT) {
            std::cerr << "unsafe conversion: float to int may cause precision loss\n";
        }
        return new_type == SYMBOL_TYPE_STRING;
    default:
        return false;
    }
}

const std::unique_ptr<FunctionSignatureAST>& SemanticAnalyzer::seek_best_match_function(const CallExprAST& expr) {
    auto candidates = ast->function_table.equal_range(expr.name);
    std::unique_ptr<FunctionSignatureAST>* current_candidate = nullptr;
    int current_mandatory = -1;
    for (auto& it = candidates.first; it != candidates.second; ++it) {
        int mandatory_args = std::count_if(it->second->signature->arguments.begin(),
            it->second->signature->arguments.end(),
            [](const std::unique_ptr<FunctionArgument>& arg) {
                return arg->default_value == nullptr;
            });
        int optional_args = it->second->signature->arguments.size() - mandatory_args;
        if (expr.arguments.size() >= mandatory_args && mandatory_args > current_mandatory) {
            current_candidate = &it->second->signature;
            current_mandatory = mandatory_args;
        }
        if (expr.arguments.size() == mandatory_args + optional_args) return it->second->signature; // best match
    }
    if (current_candidate == nullptr && ast->extern_function_table.contains(expr.name)) {
        auto& candidate = ast->extern_function_table.at(expr.name);
        if (candidate->arguments.size() == expr.arguments.size()) return candidate;
    }
    return *current_candidate;
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
        auto& candidate = seek_best_match_function(call);
        if (candidate != nullptr) return candidate->return_value_type;
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
            return call.op == TOKEN_LOGIC_NOT ? SYMBOL_TYPE_INT : type;
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
                if (rhs_type == SYMBOL_TYPE_FLOAT) {
                    std::cerr << "unsafe conversion: float to int may cause precision loss\n";
                }
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
        if ((*scope)->symbol_table.contains(var.name)) {
            auto range = (*scope)->symbol_table.equal_range(var.name);
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
    if (typeid(*expr) == typeid(ReturnExprAST)) {
        auto& ret = dynamic_cast<ReturnExprAST&>(*expr);
        SymbolType type = get_type(ret.expr);
        if (!can_convert_to(type, (*scope)->return_value_type)) throw semantic_exception("mismatched return value type");
        return type;
    }
    throw semantic_exception("unknown expression");
}

SymbolType SemanticAnalyzer::llvm_type_to_symbol_type(llvm::Type* type)
{
    switch (type->getTypeID())
    {
    case llvm::Type::IntegerTyID:
        return SYMBOL_TYPE_INT;
    case llvm::Type::FloatTyID:
        return SYMBOL_TYPE_FLOAT;
    default:
        break;
    }
}

std::string SemanticAnalyzer::readable_function_signature(const std::unique_ptr<FunctionSignatureAST>& signature)
{
    std::string result = signature->name + '(';
    for (auto& arg : signature->arguments) {
        result += arg->name;
        switch (arg->type)
        {
        case SYMBOL_TYPE_INT:result += '%'; break;
        case SYMBOL_TYPE_FLOAT:result += '#'; break;
        case SYMBOL_TYPE_STRING:result += '$'; break;
        default: break;
        }
        result += ',';
    }
    if (signature->arguments.size() > 0) result.pop_back();
    result.push_back(')');
    return result;
}

std::string SemanticAnalyzer::readable_function_signature(const std::unique_ptr<FunctionAST>& function)
{
    return std::move(readable_function_signature(function->signature));
}
