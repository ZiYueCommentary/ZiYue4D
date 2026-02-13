#include "CodeGen.h"

#include <format>
#include <ranges>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IRReader/IRReader.h>

#include "termcolor.hpp"

void CodeGen::optimize_string() {
    for (const auto& val : semantic->ast->function_table | std::views::values) {
        for (auto& expr : val->body) {
            expr = std::move(merge_literal_string_operations(std::move(expr)));
        }
    }
}

bool CodeGen::generate() {
    // initializing constants
    for (auto& [constant, value] : semantic->ast->constant_table) {
        if (semantic->ast->is_variable(constant) == SYMBOL_TYPE_STRING) {
            value = std::move(merge_literal_string_operations(std::move(value)));
            auto* variable = new llvm::GlobalVariable(
                *this->module,
                llvm::PointerType::get(*context, 0),
                false,
                llvm::GlobalValue::ExternalLinkage,
                llvm::ConstantPointerNull::get(llvm::PointerType::get(*context, 0)),
                constant
            );
            scoped_symbol_table.back().insert({constant, variable});
        } else {
            scoped_symbol_table.back().insert({constant, visit(value)});
        }
    }

    // register global variables & main entry
    for (const auto& [global, value] : semantic->ast->global_table) {
        if (semantic->ast->constant_table.contains(global)) continue;
        switch (semantic->scoped_symbol_types.front().at(global)) {
            case SYMBOL_TYPE_INT: {
                auto* variable = new llvm::GlobalVariable(
                    *this->module,
                    llvm::Type::getInt32Ty(*context),
                    false,
                    llvm::GlobalValue::ExternalLinkage,
                    llvm::ConstantInt::get(
                        *context, llvm::APInt(
                            32, is_non_string_literal_value(value) && semantic->get_type(value) == SYMBOL_TYPE_INT
                                    ? llvm::dyn_cast<llvm::ConstantInt>(visit(value))->getZExtValue()
                                    : 0, true)),
                    global
                );
                scoped_symbol_table.back().insert({global, variable});
                break;
            }
            case SYMBOL_TYPE_FLOAT: {
                auto* variable = new llvm::GlobalVariable(
                    *this->module,
                    llvm::Type::getFloatTy(*context),
                    false,
                    llvm::GlobalValue::ExternalLinkage,
                    llvm::ConstantFP::get(*context, llvm::APFloat(
                                              is_non_string_literal_value(value) && semantic->get_type(value) ==
                                              SYMBOL_TYPE_FLOAT
                                                  ? llvm::dyn_cast<llvm::ConstantFP>(visit(value))->getValueAPF().
                                                  convertToFloat()
                                                  : 0.0f)),
                    global
                );
                scoped_symbol_table.back().insert({global, variable});
                break;
            }
            case SYMBOL_TYPE_STRING: {
                auto* variable = new llvm::GlobalVariable(
                    *this->module,
                    llvm::PointerType::get(*context, 0),
                    false,
                    llvm::GlobalValue::ExternalLinkage,
                    llvm::ConstantPointerNull::get(llvm::PointerType::get(*context, 0)),
                    global
                );
                scoped_symbol_table.back().insert({global, variable});
                break;
            }
        }
    }

    // register function signatures
    for (auto& external_function_signature : semantic->ast->extern_function_table | std::views::values) {
        llvm::Function::Create(create_function_type(external_function_signature), llvm::Function::ExternalLinkage,
                               external_function_signature->name, &*module);
    }
    for (const auto& external_function : semantic->ast->function_table | std::views::values) {
        llvm::Function::Create(create_function_type(external_function->signature), llvm::Function::ExternalLinkage,
                               unique_function_name(external_function->signature), &*module);
    }

    // Initialing global variables...
    {
        const auto constructor = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getVoidTy(*context), {}, false), llvm::Function::InternalLinkage, "",
            &*module);
        llvm::BasicBlock* block = llvm::BasicBlock::Create(*context, "", constructor);
        builder->SetInsertPoint(block);
        lifecycles.push_back({false, {}});
        for (auto& [constant, value] : semantic->ast->constant_table) {
            if (semantic->ast->is_variable(constant) == SYMBOL_TYPE_STRING) {
                update_variable_value(constant, visit(value));
            }
        }
        for (auto& [global, value] : semantic->ast->global_table) {
            const SymbolType type = semantic->scoped_symbol_types.front().at(global);
            if (is_non_string_literal_value(value) && semantic->get_type(value) == type) continue;
            update_variable_value(global, cast_value_to(visit(value), type));
        }
        builder->CreateRetVoid();

        // Creating @llvm.global_ctors...
        llvm::StructType* ctorStructTy = llvm::StructType::get(
            llvm::Type::getInt32Ty(*context),
            llvm::PointerType::get(constructor->getType(), 0),
            llvm::PointerType::get(llvm::Type::getInt8Ty(*context), 0)
        );

        llvm::Constant* ctorElem = llvm::ConstantStruct::get(
            ctorStructTy,
            {
                llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), 65535),
                constructor,
                llvm::ConstantPointerNull::get(llvm::PointerType::get(*context, 0))
            }
        );

        llvm::ArrayType* arrayTy = llvm::ArrayType::get(ctorStructTy, 1);

        new llvm::GlobalVariable(
            *module,
            arrayTy,
            false,
            llvm::GlobalValue::AppendingLinkage,
            llvm::ConstantArray::get(arrayTy, {ctorElem}),
            "llvm.global_ctors"
        );
    }

    // register function definitions
    for (const auto& func_def : semantic->ast->function_table | std::views::values) {
        llvm::Function* function = module->getFunction(unique_function_name(func_def->signature));
        llvm::BasicBlock* block = llvm::BasicBlock::Create(*context, "", function);
        scoped_symbol_table.emplace_back();
        semantic->ast->scoped_symbol_table_layer.emplace_back(&func_def->signature->symbol_table);
        semantic->scoped_symbol_types.emplace_back();
        lifecycles.push_back({true, {}});
        builder->SetInsertPoint(block);
        build_scoped_symbol_table(func_def->signature->symbol_table);

        int index = 0;
        for (const auto& arg : func_def->signature->arguments) {
            function->getArg(index)->setName(arg->name);
            scoped_symbol_table.back().insert_or_assign(arg->name, function->getArg(index));
            index++;
        }
        semantic->scope_function = &func_def->signature;

        for (const auto& expr : func_def->body) {
            if (builder->GetInsertBlock()->getTerminator() != nullptr) {
                std::cerr << termcolor::yellow << "unreachable code at " << function->getName().str() << '\n' <<
                        termcolor::reset;
                break;
            }
            visit(expr);
        }
        if (builder->GetInsertBlock()->getTerminator() == nullptr) {
            release_lifecycle_resources(true);
            switch (func_def->signature->return_value_type) {
                case SYMBOL_TYPE_FLOAT:
                    builder->CreateRet(llvm::ConstantFP::get(*context, llvm::APFloat(0.0f)));
                    break;
                default:
                    builder->CreateRet(llvm::ConstantInt::get(*context, llvm::APInt(32, 0, true)));
                    break;
            }
        }
        lifecycles.pop_back();
        llvm::verifyFunction(*function, &llvm::errs());
        semantic->scope_function = nullptr;
        scoped_symbol_table.pop_back();
        semantic->ast->scoped_symbol_table_layer.pop_back();
        semantic->scoped_symbol_types.pop_back();
    }

    return true;
}

// There is no type check since I trust my semantic analyzer
llvm::Value* CodeGen::visit(const std::unique_ptr<ExprAST>& expr) {
    if (typeid(*expr) == typeid(FloatExprAST)) {
        auto& float_expr = dynamic_cast<const FloatExprAST&>(*expr);
        return llvm::ConstantFP::get(*context, llvm::APFloat(float_expr.value));
    }
    if (typeid(*expr) == typeid(IntegerExprAST)) {
        auto& int_expr = dynamic_cast<const IntegerExprAST&>(*expr);
        return llvm::ConstantInt::get(*context, llvm::APInt(32, int_expr.value, true));
    }
    if (typeid(*expr) == typeid(StringExprAST)) {
        auto& string = dynamic_cast<const StringExprAST&>(*expr);
        return build_literal_string(string.string);
    }
    if (typeid(*expr) == typeid(UnaryExprAST)) {
        auto& unary_expr = dynamic_cast<const UnaryExprAST&>(*expr);
        if (unary_expr.op == '&') {
            auto& ident = dynamic_cast<const VariableExprAST&>(*unary_expr.expr);
            const auto [function, _] = semantic->ast->function_table.equal_range(ident.name);
            return module->getFunction(unique_function_name(function->second->signature));
        }
        llvm::Value* value = visit(unary_expr.expr);
        const SymbolType type = semantic->get_type(unary_expr.expr);
        switch (unary_expr.op) {
            case TOKEN_LOGIC_NOT:
                return cast_value_to(builder->CreateICmpEQ(cast_value_to(value, SYMBOL_TYPE_INT), builder->getInt32(0)),
                                     SYMBOL_TYPE_INT);
            case '-': {
                if (type == SYMBOL_TYPE_INT) {
                    return builder->CreateSub(builder->getInt32(0), value);
                }
                return builder->CreateFSub(llvm::ConstantFP::get(value->getType(), 0.0f), value);
            }
        }
    }
    if (typeid(*expr) == typeid(BinaryExprAST)) {
        auto& bi_expr = dynamic_cast<const BinaryExprAST&>(*expr);
        llvm::Value* rhs = visit(bi_expr.rhs);
        const SymbolType lhs_type = semantic->get_type(bi_expr.lhs);
        const SymbolType rhs_type = semantic->get_type(bi_expr.rhs);
        if (bi_expr.op == '=') {
            if (typeid(*bi_expr.lhs) == typeid(VariableExprAST)) {
                auto& var = dynamic_cast<const VariableExprAST&>(*bi_expr.lhs);
                update_variable_value(var.name, cast_value_to(rhs, semantic->get_type(bi_expr.lhs)));
            }
            return rhs;
        }
        llvm::Value* lhs = visit(bi_expr.lhs);
        if (lhs_type == SYMBOL_TYPE_STRING || rhs_type == SYMBOL_TYPE_STRING) {
            llvm::Value* new_lhs = cast_value_to(lhs, SYMBOL_TYPE_STRING);
            llvm::Value* new_rhs = cast_value_to(rhs, SYMBOL_TYPE_STRING);
            if (bi_expr.op == '+') {
                llvm::Value* new_string = builder->
                        CreateCall(module->getFunction("ziyue4d_Concat"), {new_lhs, new_rhs});
                lifecycles.back().values.insert(new_string);
                return new_string;
            }
            if (bi_expr.op == TOKEN_EQUALS) {
                return cast_value_to(
                    builder->CreateCall(module->getFunction("ziyue4d_StringEquals"), {new_lhs, new_rhs}),
                    SYMBOL_TYPE_INT);
            }
            return nullptr;
        }
        if (semantic->is_bitwise_or_logic_operator(bi_expr.op)) {
            llvm::Value* new_lhs = cast_value_to(lhs, SYMBOL_TYPE_INT);
            llvm::Value* new_rhs = cast_value_to(rhs, SYMBOL_TYPE_INT);
            switch (bi_expr.op) {
                case TOKEN_LOGIC_AND: {
                    llvm::Value* bool_lhs = builder->CreateICmpNE(new_lhs, builder->getInt32(0));
                    llvm::Value* bool_rhs = builder->CreateICmpNE(new_rhs, builder->getInt32(0));
                    return cast_value_to(builder->CreateAnd(bool_lhs, bool_rhs), SYMBOL_TYPE_INT);
                }
                case TOKEN_LOGIC_OR: {
                    llvm::Value* bool_lhs = builder->CreateICmpNE(new_lhs, builder->getInt32(0));
                    llvm::Value* bool_rhs = builder->CreateICmpNE(new_rhs, builder->getInt32(0));
                    return cast_value_to(builder->CreateOr(bool_lhs, bool_rhs), SYMBOL_TYPE_INT);
                }
                case '&':
                    return builder->CreateAnd(new_lhs, new_rhs);
                case TOKEN_BITWISE_OR:
                    return builder->CreateOr(new_lhs, new_rhs);
            }
            return nullptr;
        }
        if (lhs_type == SYMBOL_TYPE_FLOAT || rhs_type == SYMBOL_TYPE_FLOAT) {
            llvm::Value* new_lhs = cast_value_to(lhs, SYMBOL_TYPE_FLOAT);
            llvm::Value* new_rhs = cast_value_to(rhs, SYMBOL_TYPE_FLOAT);
            switch (bi_expr.op) {
                case '+':
                    return builder->CreateFAdd(new_lhs, new_rhs);
                case '-':
                    return builder->CreateFSub(new_lhs, new_rhs);
                case '*':
                    return builder->CreateFMul(new_lhs, new_rhs);
                case '/':
                    return builder->CreateFDiv(new_lhs, new_rhs);
                case TOKEN_EQUALS:
                    return cast_value_to(builder->CreateFCmpOEQ(new_lhs, new_rhs), SYMBOL_TYPE_INT);
                case TOKEN_NOT_EQUALS:
                    return cast_value_to(builder->CreateFCmpONE(new_lhs, new_rhs), SYMBOL_TYPE_INT);
                case TOKEN_LESS_THAN:
                    return cast_value_to(builder->CreateFCmpOLT(new_lhs, new_rhs), SYMBOL_TYPE_INT);
                case TOKEN_LESS_THAN_OR_EQUALS:
                    return cast_value_to(builder->CreateFCmpOLE(new_lhs, new_rhs), SYMBOL_TYPE_INT);
                case TOKEN_GREATER_THAN:
                    return cast_value_to(builder->CreateFCmpOGT(new_lhs, new_rhs), SYMBOL_TYPE_INT);
                case TOKEN_GREATER_THAN_OR_EQUALS:
                    return cast_value_to(builder->CreateFCmpOGE(new_lhs, new_rhs), SYMBOL_TYPE_INT);
            }
        }
        switch (bi_expr.op) {
            case '+':
                return builder->CreateAdd(lhs, rhs);
            case '-':
                return builder->CreateSub(lhs, rhs);
            case '*':
                return builder->CreateMul(lhs, rhs);
            case '/':
                return builder->CreateSDiv(lhs, rhs);
            case TOKEN_EQUALS:
                return cast_value_to(builder->CreateICmpEQ(lhs, rhs), SYMBOL_TYPE_INT);
            case TOKEN_NOT_EQUALS:
                return cast_value_to(builder->CreateICmpNE(lhs, rhs), SYMBOL_TYPE_INT);
            case TOKEN_LESS_THAN:
                return cast_value_to(builder->CreateICmpSLT(lhs, rhs), SYMBOL_TYPE_INT);
            case TOKEN_LESS_THAN_OR_EQUALS:
                return cast_value_to(builder->CreateICmpSLE(lhs, rhs), SYMBOL_TYPE_INT);
            case TOKEN_GREATER_THAN:
                return cast_value_to(builder->CreateICmpSGT(lhs, rhs), SYMBOL_TYPE_INT);
            case TOKEN_GREATER_THAN_OR_EQUALS:
                return cast_value_to(builder->CreateICmpSGE(lhs, rhs), SYMBOL_TYPE_INT);
        }
    }
    if (typeid(*expr) == typeid(VariableExprAST)) {
        auto& var = dynamic_cast<const VariableExprAST&>(*expr);
        return find_variable_value(var.name);
    }
    if (typeid(*expr) == typeid(CallExprAST)) {
        auto& call = dynamic_cast<const CallExprAST&>(*expr);
        auto& func = *semantic->seek_best_match_function(call);
        std::vector<llvm::Value*> built_arguments = {};
        for (int i = 0; i < func->arguments.size(); i++) {
            built_arguments.push_back(cast_value_to(
                visit(call.arguments.size() > i ? call.arguments.at(i) : func->arguments.at(i)->default_value),
                func->arguments.at(i)->type));
        }
        llvm::Value* ret_val = builder->CreateCall(module->getFunction(unique_function_name(func)), built_arguments);
        if (func->return_value_type == SYMBOL_TYPE_STRING) lifecycles.back().values.insert(ret_val);
        return ret_val;
    }
    if (typeid(*expr) == typeid(ReturnExprAST)) {
        auto& ret = dynamic_cast<const ReturnExprAST&>(*expr);
        llvm::Value* return_value = nullptr;
        if (ret.expr == nullptr) {
            switch ((*semantic->scope_function)->return_value_type) {
                case SYMBOL_TYPE_FLOAT:
                    return_value = llvm::ConstantFP::get(*context, llvm::APFloat(0.0f));
                    break;
                case SYMBOL_TYPE_STRING:
                    return_value = build_literal_string("");
                    break;
                default:
                    return_value = llvm::ConstantInt::get(*context, llvm::APInt(32, 0, true));
                    break;
            }
        } else {
            return_value = cast_value_to(visit(ret.expr), (*semantic->scope_function)->return_value_type);
        }
        release_lifecycle_resources(true, return_value);
        builder->CreateRet(return_value);
    }
    // dangerous!
    static llvm::BasicBlock *pre_while_block = nullptr, *post_while_block = nullptr;
    if (typeid(*expr) == typeid(IfStatementAST)) {
        auto& if_statement = dynamic_cast<const IfStatementAST&>(*expr);
        llvm::Value* condition = builder->CreateICmpNE(visit(if_statement.condition), builder->getInt32(0));
        llvm::Function* scope = module->getFunction(unique_function_name(*semantic->scope_function));
        llvm::BasicBlock* statement_true_block = llvm::BasicBlock::Create(*context, "", scope);
        llvm::BasicBlock* statement_false_block = llvm::BasicBlock::Create(*context, "", scope);
        llvm::BasicBlock* post_if_statement = llvm::BasicBlock::Create(*context, "", scope);
        builder->CreateCondBr(condition, statement_true_block, statement_false_block);
        // statement true
        semantic->scoped_symbol_types.emplace_back();
        scoped_symbol_table.emplace_back();
        build_scoped_symbol_table(if_statement.statement_true_symbol_table);
        builder->SetInsertPoint(statement_true_block);
        lifecycles.push_back({false, {}});
        for (const auto& if_expr : if_statement.statement_true) {
            if (builder->GetInsertBlock()->getTerminator() != nullptr) break;
            if (typeid(*if_expr) == typeid(ExitAST)) {
                builder->CreateBr(post_while_block);
                continue;
            }
            if (typeid(*if_expr) == typeid(ContinueAST)) {
                builder->CreateBr(pre_while_block);
                continue;
            }
            visit(if_expr);
        }
        if (builder->GetInsertBlock()->getTerminator() == nullptr) {
            release_lifecycle_resources();
            builder->CreateBr(post_if_statement);
        }
        lifecycles.pop_back();
        scoped_symbol_table.pop_back();
        semantic->scoped_symbol_types.pop_back();
        // statement false
        semantic->scoped_symbol_types.emplace_back();
        scoped_symbol_table.emplace_back();
        builder->SetInsertPoint(statement_false_block);
        build_scoped_symbol_table(if_statement.statement_false_symbol_table);
        lifecycles.push_back({false, {}});
        for (const auto& if_expr : if_statement.statement_false) {
            if (builder->GetInsertBlock()->getTerminator() != nullptr) break;
            if (typeid(*if_expr) == typeid(ExitAST)) {
                builder->CreateBr(post_while_block);
                continue;
            }
            if (typeid(*if_expr) == typeid(ContinueAST)) {
                builder->CreateBr(pre_while_block);
                continue;
            }
            visit(if_expr);
        }
        if (builder->GetInsertBlock()->getTerminator() == nullptr) {
            release_lifecycle_resources();
            builder->CreateBr(post_if_statement);
        }
        lifecycles.pop_back();
        scoped_symbol_table.pop_back();
        semantic->scoped_symbol_types.pop_back();

        builder->SetInsertPoint(post_if_statement);
    }
    if (typeid(*expr) == typeid(WhileStatementAST)) {
        auto& while_statement = dynamic_cast<const WhileStatementAST&>(*expr);
        llvm::Function* scope = module->getFunction(unique_function_name(*semantic->scope_function));
        pre_while_block = nullptr;
        if (scope->back().empty()) {
            pre_while_block = &scope->back();
        } else {
            pre_while_block = llvm::BasicBlock::Create(*context, "", scope);
            builder->CreateBr(pre_while_block);
            builder->SetInsertPoint(pre_while_block);
        }
        llvm::Value* condition = builder->CreateICmpNE(visit(while_statement.condition), builder->getInt32(0));
        llvm::BasicBlock* statement_true_block = llvm::BasicBlock::Create(*context, "", scope);
        post_while_block = llvm::BasicBlock::Create(*context, "", scope);
        builder->CreateCondBr(condition, statement_true_block, post_while_block);
        semantic->scoped_symbol_types.emplace_back();
        scoped_symbol_table.emplace_back();
        builder->SetInsertPoint(statement_true_block);
        build_scoped_symbol_table(while_statement.statement_true_symbol_table);
        lifecycles.push_back({false, {}});
        for (const auto& while_expr : while_statement.statement_true) {
            if (builder->GetInsertBlock()->getTerminator() != nullptr) break;
            if (typeid(*while_expr) == typeid(ExitAST)) {
                builder->CreateBr(post_while_block);
                continue;
            }
            if (typeid(*while_expr) == typeid(ContinueAST)) {
                builder->CreateBr(pre_while_block);
                continue;
            }
            visit(while_expr);
        }
        if (builder->GetInsertBlock()->getTerminator() == nullptr) {
            release_lifecycle_resources();
            builder->CreateBr(pre_while_block);
        }
        lifecycles.pop_back();
        scoped_symbol_table.pop_back();
        semantic->scoped_symbol_types.pop_back();
        builder->SetInsertPoint(post_while_block);
    }
    return nullptr;
}

llvm::Value* CodeGen::cast_value_to(llvm::Value* value, SymbolType type) {
    switch (value->getType()->getTypeID()) {
        case llvm::Type::IntegerTyID:
            switch (type) {
                case SYMBOL_TYPE_POINTER:
                case SYMBOL_TYPE_STRING: {
                    llvm::Value* new_value = builder->CreateCall(module->getFunction("ziyue4d_int_to_string__"),
                                                                 {value});
                    lifecycles.back().values.insert(new_value);
                    return new_value;
                }
                case SYMBOL_TYPE_FLOAT:
                    return builder->CreateSIToFP(value, llvm::Type::getFloatTy(*context));
                case SYMBOL_TYPE_INT:
                    if (value->getType() == builder->getInt1Ty()) {
                        // bool to int32
                        return builder->CreateZExt(value, builder->getInt32Ty());
                    }
                default:
                    return value;
            }
        case llvm::Type::FloatTyID:
            switch (type) {
                case SYMBOL_TYPE_POINTER:
                case SYMBOL_TYPE_STRING: {
                    llvm::Value* new_value = builder->CreateCall(module->getFunction("ziyue4d_float_to_string__"),
                                                                 {value});
                    lifecycles.back().values.insert(new_value);
                    return new_value;
                }
                case SYMBOL_TYPE_INT:
                    return builder->CreateFPToSI(builder->CreateCall(module->getFunction("ziyue4d_Round"), value),
                                                 llvm::Type::getInt32Ty(*context));
                default:
                    return value;
            }
    }
    return value;
}

llvm::FunctionType* CodeGen::create_function_type(const std::unique_ptr<FunctionSignatureAST>& signature) {
    std::vector<llvm::Type*> arguments{signature->arguments.size()};
    std::ranges::transform(signature->arguments,
                           arguments.begin(),
                           [this](const std::unique_ptr<FunctionArgument>& arg) {
                               return symbol_type_to_type(arg->type);
                           });
    return llvm::FunctionType::get(symbol_type_to_type(signature->return_value_type), arguments, false);
}

llvm::Type* CodeGen::token_to_type(const Token token) const {
    switch (token) {
        case TOKEN_TYPE_INT:
            return llvm::Type::getInt32Ty(*context);
        case TOKEN_TYPE_FLOAT:
            return llvm::Type::getFloatTy(*context);
        case TOKEN_TYPE_STRING:
            return llvm::PointerType::get(*context, 0);
        default:
            break;
    }
}

llvm::Type* CodeGen::symbol_type_to_type(SymbolType type) const {
    switch (type) {
        case SYMBOL_TYPE_INT:
            return llvm::Type::getInt32Ty(*context);
        case SYMBOL_TYPE_FLOAT:
            return llvm::Type::getFloatTy(*context);
        case SYMBOL_TYPE_STRING:
            return llvm::PointerType::get(*context, 0);
        case SYMBOL_TYPE_VOID:
            return llvm::Type::getVoidTy(*context);
        case SYMBOL_TYPE_FUNCTION:
        case SYMBOL_TYPE_STRUCT:
        default:
            return llvm::PointerType::get(*context, 0);
    }
}

// SymbolType CodeGen::llvm_value_symbol_type(llvm::Value* value) const {
//     switch (value->getType()->getTypeID()) {
//         case llvm::Type::IntegerTyID: return SYMBOL_TYPE_INT;
//         case llvm::Type::FloatTyID: return SYMBOL_TYPE_FLOAT;
//         case llvm::Type::PointerTyID: return SYMBOL_TYPE_POINTER;
//         default: throw codegen_exception("unknown llvm value type");
//     }
//}

std::string CodeGen::unique_function_name(const std::unique_ptr<FunctionSignatureAST>& signature) {
    static std::map<void*, std::string> cache = {}; // This is an unsafe practice.
    if (signature->name == "main") return "main";
    if (cache.contains((void*) &signature)) return cache.at((void*) &signature);

    auto extern_func = semantic->ast->extern_function_table.find(
        signature->name.starts_with("ziyue4d_")
            ? to_lower_string(signature->name.substr(8))
            : to_lower_string(signature->name));
    if (extern_func != semantic->ast->extern_function_table.end() && extern_func->second == signature) {
        cache.insert({(void*) &signature, signature->name});
        return cache.at((void*) &signature);
    }

    int mandatory_args = std::ranges::count_if(signature->arguments,
                                               [](const std::unique_ptr<FunctionArgument>& arg) {
                                                   return arg->default_value == nullptr;
                                               });
    int optional_args = signature->arguments.size() - mandatory_args;
    char return_value_type = 'i';
    switch (signature->return_value_type) {
        case SYMBOL_TYPE_FLOAT:
            return_value_type = 'f';
            break;
        case SYMBOL_TYPE_STRING:
            return_value_type = 's';
            break;
        case SYMBOL_TYPE_STRUCT:
            return_value_type = 'p';
    }

    std::string&& stylized = std::move(std::format("{}{}_{}_{}", return_value_type, signature->name, mandatory_args,
                                                   optional_args));
    cache.insert({(void*) &signature, stylized});

    return cache.at((void*) &signature);
}

void CodeGen::update_variable_value(const std::string& name, llvm::Value* value) {
    for (auto& table : std::ranges::reverse_view(scoped_symbol_table)) {
        if (table.contains(name)) {
            for (const auto& type_table : std::ranges::reverse_view(semantic->scoped_symbol_types)) {
                if (type_table.contains(name) && type_table.at(name) == SYMBOL_TYPE_STRING &&
                    type_table != semantic->scoped_symbol_types.front()) {
                    table.insert_or_assign(name, value);
                    return;
                }
            }
            builder->CreateStore(value, table.at(name));
            return;
        }
    }
}

llvm::Value* CodeGen::find_variable_value(const std::string& name) {
    for (const auto& table : std::ranges::reverse_view(scoped_symbol_table)) {
        if (table.contains(name)) {
            for (const auto& type_table : std::ranges::reverse_view(semantic->scoped_symbol_types)) {
                if (type_table.contains(name)) {
                    if (type_table.at(name) == SYMBOL_TYPE_STRING &&
                        type_table != semantic->scoped_symbol_types.front())
                        return table.at(name);
                    return builder->CreateLoad(symbol_type_to_type(type_table.at(name)), table.at(name));
                }
            }
        }
    }
    if (semantic->ast->constant_table.contains(name) && semantic->ast->is_variable(name) != SYMBOL_TYPE_STRING) {
        // constant
        return scoped_symbol_table.front().at(name);
    }
}

void CodeGen::release_lifecycle_resources(const bool is_function_return, const llvm::Value* return_value) {
    for (const auto& [is_function, values] : std::ranges::reverse_view(lifecycles)) {
        for (auto value : values) {
            if (value != return_value) {
                builder->CreateCall(module->getFunction("ziyue4d_release_string__"), {value});
            }
        }
        if (!is_function_return || is_function) break;
    }
}

llvm::Value* CodeGen::build_literal_string(const std::string& str) {
    static std::unordered_map<std::string, llvm::Constant*> global_string_ptrs = {};
    if (!global_string_ptrs.contains(str)) {
        llvm::GlobalVariable* global_string = builder->CreateGlobalString(str);
        global_string_ptrs.insert({str, global_string});
    }
    llvm::Value* built_string = builder->CreateCall(module->getFunction("ziyue4d_create_string__"),
                                                    {global_string_ptrs.at(str)});
    lifecycles.back().values.insert(built_string);
    return built_string;
}

std::unique_ptr<ExprAST> CodeGen::merge_literal_string_operations(std::unique_ptr<ExprAST> expr) {
    if (typeid(*expr) == typeid(CallExprAST)) {
        for (auto& call = dynamic_cast<CallExprAST&>(*expr); auto& arg : call.arguments) {
            arg = merge_literal_string_operations(std::move(arg));
        }
    }
    if (typeid(*expr) == typeid(BinaryExprAST)) {
        auto& biexpr = dynamic_cast<BinaryExprAST&>(*expr);
        biexpr.lhs = std::move(merge_literal_string_operations(std::move(biexpr.lhs)));
        biexpr.rhs = std::move(merge_literal_string_operations(std::move(biexpr.rhs)));
        if (typeid(*biexpr.lhs) == typeid(StringExprAST) || typeid(*biexpr.rhs) == typeid(StringExprAST)) {
            if (is_literal_expression(*biexpr.lhs) && is_literal_expression(*biexpr.rhs)) {
                std::string lhs_literal = literal_to_string(*biexpr.lhs);
                std::string rhs_literal = literal_to_string(*biexpr.rhs);
                return std::make_unique<StringExprAST>(std::move(lhs_literal + rhs_literal),
                                                       llvm::SMRange(biexpr.lhs->range.Start, biexpr.rhs->range.End));
            }
        }
    }
    return expr;
}

bool CodeGen::is_literal_expression(const ExprAST& expr) {
    const auto& ty = typeid(expr);
    return ty == typeid(StringExprAST) || ty == typeid(IntegerExprAST) || ty == typeid(FloatExprAST);
}

bool CodeGen::is_non_string_literal_value(const std::unique_ptr<ExprAST>& expr) const {
    if (typeid(*expr) == typeid(IntegerExprAST) || typeid(*expr) == typeid(FloatExprAST)) return true;
    if (typeid(*expr) == typeid(BinaryExprAST)) {
        const auto& bi_expr = dynamic_cast<const BinaryExprAST&>(*expr);
        return is_non_string_literal_value(bi_expr.lhs) && is_non_string_literal_value(bi_expr.rhs);
    }
    if (typeid(*expr) == typeid(UnaryExprAST)) {
        const auto& ary_expr = dynamic_cast<const UnaryExprAST&>(*expr);
        return is_non_string_literal_value(ary_expr.expr);
    }
    return false;
}

std::string CodeGen::literal_to_string(const ExprAST& expr) {
    if (typeid(expr) == typeid(StringExprAST)) {
        auto& str = dynamic_cast<const StringExprAST&>(expr);
        return str.string;
    }
    if (typeid(expr) == typeid(IntegerExprAST)) {
        auto& integer = dynamic_cast<const IntegerExprAST&>(expr);
        return std::to_string(integer.value);
    }
    if (typeid(expr) == typeid(FloatExprAST)) {
        auto& flt = dynamic_cast<const FloatExprAST&>(expr);
        return std::to_string(flt.value);
    }
    return std::string();
}

void CodeGen::build_scoped_symbol_table(const SymbolTable& symbol_table) {
    for (const auto& [name, type] : symbol_table) {
        if (semantic->ast->is_variable(name)) {
            semantic->scoped_symbol_types.back().emplace(name, type);
            switch (type) {
                case SYMBOL_TYPE_INT:
                    scoped_symbol_table.back().insert({
                        name, builder->CreateAlloca(builder->getInt32Ty(), nullptr, name)
                    });
                    break;
                case SYMBOL_TYPE_FLOAT:
                    scoped_symbol_table.back().insert({
                        name, builder->CreateAlloca(builder->getFloatTy(), nullptr, name)
                    });
                    break;
                case SYMBOL_TYPE_STRING:
                    scoped_symbol_table.back().insert({name, build_literal_string("")});
                    break;
                case SYMBOL_TYPE_POINTER:
                    scoped_symbol_table.back().insert({
                        name, llvm::ConstantPointerNull::get(llvm::PointerType::get(*context, 0))
                    });
            }
        }
    }
}

std::string CodeGen::to_lower_string(const std::string& str) {
    std::string result{};
    result.reserve(str.length());
    for (size_t i = 0; i < str.length(); i++) {
        result.push_back(std::tolower(str.at(i)));
    }
    return result;
}

std::error_code Compiler::write_file(const std::string& file) {
    std::error_code err;
    if (file.empty()) {
        module->print(llvm::outs(), nullptr);
    } else {
        llvm::raw_fd_ostream ofstream{file, err};
        module->print(ofstream, nullptr);
    }
    return err;
}
