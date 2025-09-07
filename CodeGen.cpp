#include "CodeGen.h"
#include <llvm/IR/Verifier.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>

#ifdef _WIN32
#pragma comment(linker, "/export:??_7type_info@@6B@")
#pragma comment(linker, "/export:??_U@YAPEAX_K@Z")
#pragma comment(linker, "/export:??_V@YAXPEAX@Z")
#pragma comment(linker, "/export:??3@YAXPEAX_K@Z")
#endif

llvm::Value* CodeGen::generate_functions()
{
    // register global variables & main entry
    for (const auto& symbol : semantic->ast->global_symbols) {
        switch (symbol.second) {
        case SYMBOL_TYPE_INT:
        {
            llvm::GlobalVariable* variable = new llvm::GlobalVariable(
                *this->module,
                llvm::Type::getInt32Ty(*context),
                false,
                llvm::GlobalValue::ExternalLinkage,
                llvm::ConstantInt::get(*context, llvm::APInt(32, 0, true)),
                symbol.first
            );
            variable->print(llvm::errs());
            llvm::errs() << '\n';
            scoped_symbol_table.back().insert({ symbol.first, variable });
            break;
        }
        case SYMBOL_TYPE_FLOAT:
        {
            llvm::GlobalVariable* variable = new llvm::GlobalVariable(
                *this->module,
                llvm::Type::getFloatTy(*context),
                false,
                llvm::GlobalValue::ExternalLinkage,
                llvm::ConstantFP::get(*context, llvm::APFloat(0.0f)),
                symbol.first
            );
            variable->print(llvm::errs());
            llvm::errs() << '\n';
            scoped_symbol_table.back().insert({ symbol.first, variable });
            break;
        }
        }
    }

    // register function signatures
    for (auto& func : semantic->ast->extern_function_table) {
        llvm::Function::Create(create_function_type(func.second), llvm::Function::ExternalLinkage, func.second->name, &*module)->print(llvm::errs());
    }
    for (auto& func : semantic->ast->function_table) {
        llvm::Function* function = llvm::Function::Create(create_function_type(func.second->signature), llvm::Function::ExternalLinkage, unique_function_name(func.second->signature), &*module);
    }

    // register function definations
    for (auto& func : semantic->ast->function_table) {
        llvm::Function* function = module->getFunction(unique_function_name(func.second->signature));
        llvm::BasicBlock* block = llvm::BasicBlock::Create(*context, "", function);
        scoped_symbol_table.push_back({});
        lifecycles.push({ true, {} });
        builder->SetInsertPoint(block);
        for (const auto& symbol : func.second->signature->symbol_table) {
            switch (symbol.second) {
            case SYMBOL_TYPE_INT:
                scoped_symbol_table.back().insert({ symbol.first, llvm::ConstantInt::get(*context, llvm::APInt(32, 0, true)) });
                break;
            case SYMBOL_TYPE_FLOAT:
                scoped_symbol_table.back().insert({ symbol.first, llvm::ConstantFP::get(*context, llvm::APFloat(0.0f)) });
                break;
            case SYMBOL_TYPE_STRING:
                scoped_symbol_table.back().insert({ symbol.first, build_literal_string("") });
                break;
            }
        }
        int index = 0;
        for (const auto& arg : func.second->signature->arguments) {
            function->getArg(index)->setName(arg->name);
            scoped_symbol_table.back().insert_or_assign(arg->name, function->getArg(index));
            index++;
        }
        semantic->scope = &func.second->signature;
        for (const auto& expr : func.second->body) {
            if (builder->GetInsertBlock()->getTerminator() != nullptr) {
                llvm::errs() << "unreachable code\n";
                break;
            }
            visit(expr);
        }
        if (builder->GetInsertBlock()->getTerminator() == nullptr) {
            release_lifecycle_resources(true);
            switch (func.second->signature->return_value_type) {
            case SYMBOL_TYPE_FLOAT:
                builder->CreateRet(llvm::ConstantFP::get(*context, llvm::APFloat(0.0f)));
                break;
            default:
                builder->CreateRet(llvm::ConstantInt::get(*context, llvm::APInt(32, 0, true)));
                break;
            }
        }
        llvm::verifyFunction(*function);
        function->print(llvm::errs());
        semantic->scope = nullptr;
        scoped_symbol_table.pop_back();
    }
    return nullptr;
}

// There is no type check since I trust my semantic analyzer
llvm::Value* CodeGen::visit(const std::unique_ptr<ExprAST>& expr)
{
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
        llvm::Value* value = visit(unary_expr.expr);
        SymbolType type = semantic->get_type(unary_expr.expr);
        switch (unary_expr.op) {
        case TOKEN_LOGIC_NOT:
            if (type == SYMBOL_TYPE_INT) {
                return cast_value_to(builder->CreateICmpEQ(value, llvm::ConstantInt::get(value->getType(), 0)), SYMBOL_TYPE_INT);
            }
            else {
                return cast_value_to(builder->CreateFCmpOEQ(value, llvm::ConstantFP::get(value->getType(), 0.0f)), SYMBOL_TYPE_INT);
            }
        case '-':
            if (type == SYMBOL_TYPE_INT) {
                return builder->CreateSub(llvm::ConstantInt::get(value->getType(), 0), value);
            }
            else {
                return builder->CreateFSub(llvm::ConstantFP::get(value->getType(), 0.0f), value);
            }
        }
    }
    if (typeid(*expr) == typeid(BinaryExprAST)) {
        auto& bi_expr = dynamic_cast<const BinaryExprAST&>(*expr);
        llvm::Value* lhs = visit(bi_expr.lhs);
        llvm::Value* rhs = visit(bi_expr.rhs);
        SymbolType lhs_type = semantic->get_type(bi_expr.lhs);
        SymbolType rhs_type = semantic->get_type(bi_expr.rhs);
        switch (bi_expr.op)
        {
        case '+':
        case '-':
        case '*':
        case '/':
            if (lhs_type == SYMBOL_TYPE_STRING || rhs_type == SYMBOL_TYPE_STRING) {
                llvm::Value* new_lhs = lhs;
                llvm::Value* new_rhs = rhs;
                if (lhs_type == SYMBOL_TYPE_INT) {
                    new_lhs = builder->CreateCall(module->getFunction("_ziyue4d_int_to_string__"), { lhs });
                    lifecycles.top().values.insert(new_lhs);
                }
                if (lhs_type == SYMBOL_TYPE_FLOAT) {
                    new_lhs = builder->CreateCall(module->getFunction("_ziyue4d_float_to_string__"), { lhs });
                    lifecycles.top().values.insert(new_lhs);
                }
                if (rhs_type == SYMBOL_TYPE_INT) {
                    new_rhs = builder->CreateCall(module->getFunction("_ziyue4d_int_to_string__"), { rhs });
                    lifecycles.top().values.insert(new_rhs);
                }
                if (rhs_type == SYMBOL_TYPE_FLOAT) {
                    new_rhs = builder->CreateCall(module->getFunction("_ziyue4d_float_to_string__"), { rhs });
                    lifecycles.top().values.insert(new_rhs);
                }
                if (bi_expr.op == '+') {
                    llvm::Value* new_string = builder->CreateCall(module->getFunction("_ziyue4d_concat"), { new_lhs, new_rhs });
                    lifecycles.top().values.insert(new_string);
                    return new_string;
                }
                return nullptr;
            }

            if (lhs_type == SYMBOL_TYPE_FLOAT || rhs_type == SYMBOL_TYPE_FLOAT) {
                llvm::Value* new_lhs = lhs;
                llvm::Value* new_rhs = rhs;
                if (lhs_type == SYMBOL_TYPE_INT) {
                    new_lhs = builder->CreateSIToFP(lhs, llvm::Type::getFloatTy(*context));
                }
                if (rhs_type == SYMBOL_TYPE_INT) {
                    new_rhs = builder->CreateSIToFP(rhs, llvm::Type::getFloatTy(*context));
                }
                switch (bi_expr.op) {
                case '+':
                    return builder->CreateFAdd(new_lhs, new_rhs);
                case '-':
                    return builder->CreateFSub(new_lhs, new_rhs);
                case '*':
                    return builder->CreateFMul(new_lhs, new_rhs);
                case '/':
                    return builder->CreateFDiv(new_lhs, new_rhs);
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
            }
        case '=':
            if (typeid(*bi_expr.lhs) == typeid(VariableExprAST)) {
                auto& var = dynamic_cast<const VariableExprAST&>(*bi_expr.lhs);
                update_variable_value(var.name, cast_value_to(rhs, semantic->get_type(bi_expr.lhs)));
            }
            return rhs;
        }
    }
    if (typeid(*expr) == typeid(VariableExprAST)) {
        auto& var = dynamic_cast<const VariableExprAST&>(*expr);
        return find_variable_value(var.name);
    }
    if (typeid(*expr) == typeid(CallExprAST)) {
        auto& call = dynamic_cast<const CallExprAST&>(*expr);
        auto& func = semantic->seek_best_match_function(call);
        std::vector<llvm::Value*> built_arguments = {};
        for (int i = 0; i < func->arguments.size(); i++)
        {
            built_arguments.push_back(cast_value_to(
                visit(call.arguments.size() > i ? call.arguments.at(i) : func->arguments.at(i)->default_value),
                func->arguments.at(i)->type));
        }
        llvm::Value* ret_val = builder->CreateCall(module->getFunction(unique_function_name(func)), built_arguments);
        if (func->return_value_type == SYMBOL_TYPE_STRING) lifecycles.top().values.insert(ret_val);
        return ret_val;
    }
    if (typeid(*expr) == typeid(ReturnExprAST)) {
        auto& ret = dynamic_cast<const ReturnExprAST&>(*expr);
        llvm::Value* return_value = cast_value_to(visit(ret.expr), (*semantic->scope)->return_value_type);
        release_lifecycle_resources(true, return_value);
        builder->CreateRet(return_value);
    }
    return nullptr;
}

llvm::Value* CodeGen::cast_value_to(llvm::Value* value, SymbolType type)
{
    switch (value->getType()->getTypeID()) {
    case llvm::Type::IntegerTyID:
        switch (type) {
        case SYMBOL_TYPE_FLOAT:
            return builder->CreateSIToFP(value, llvm::Type::getFloatTy(*context));
        case SYMBOL_TYPE_STRING:
        case SYMBOL_TYPE_INT:
            if (value->getType() == builder->getInt1Ty()) { // bool to int32
                return builder->CreateZExt(value, builder->getInt32Ty());
            }
        default:
            return value;
        }
    case llvm::Type::FloatTyID:
        switch (type) {
        case SYMBOL_TYPE_INT:
            return builder->CreateFPToSI(value, llvm::Type::getInt32Ty(*context));
        case SYMBOL_TYPE_STRING:
        default:
            return value;
        }
    }
    return value;
}

llvm::FunctionType* CodeGen::create_function_type(const std::unique_ptr<FunctionSignatureAST>& signature)
{
    std::vector<llvm::Type*> arguments{ signature->arguments.size() };
    std::transform(signature->arguments.begin(),
        signature->arguments.end(),
        arguments.begin(),
        [this](const std::unique_ptr<FunctionArgument>& arg) { return symbol_type_to_type(arg->type); });
    return llvm::FunctionType::get(symbol_type_to_type(signature->return_value_type), arguments, false);
}

llvm::Type* CodeGen::token_to_type(Token token)
{
    switch (token)
    {
    case TOKEN_TYPE_INT:
        return llvm::Type::getInt32Ty(*context);
    case TOKEN_TYPE_FLOAT:
        return llvm::Type::getFloatTy(*context);
    case TOKEN_TYPE_STRING:
        return llvm::PointerType::get(*context, 0);
        break;
    default:
        break;
    }
}

llvm::Type* CodeGen::symbol_type_to_type(SymbolType type)
{
    switch (type)
    {
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

std::string CodeGen::unique_function_name(const std::unique_ptr<FunctionSignatureAST>& signature)
{
    static std::map<void*, std::string> cache = {};
    if (signature->name == "__main") return "__main";
    if (cache.contains((void*)&signature)) return cache.at((void*)&signature); // what am i doing?

    auto extern_func = semantic->ast->extern_function_table.find(signature->name.starts_with("_ziyue4d_") ? signature->name.substr(9) : signature->name);
    if (extern_func != semantic->ast->extern_function_table.end() && extern_func->second == signature) {
        cache.insert({ (void*)&signature, signature->name });
        return cache.at((void*)&signature);
    }

    int mandatory_args = std::count_if(signature->arguments.begin(),
        signature->arguments.end(),
        [](const std::unique_ptr<FunctionArgument>& arg) {
            return arg->default_value == nullptr;
        });
    int optional_args = signature->arguments.size() - mandatory_args;
    char return_value_type = 'i';
    switch (signature->return_value_type)
    {
    case SYMBOL_TYPE_FLOAT:
        return_value_type = 'f';
        break;
    case SYMBOL_TYPE_STRING:
        return_value_type = 's';
        break;
    case SYMBOL_TYPE_STRUCT:
        return_value_type = 'p';
    }

    std::string&& stylized = std::move(std::format("{}{}_{}_{}", return_value_type, signature->name, mandatory_args, optional_args));
    cache.insert({ (void*)&signature, stylized });

    return cache.at((void*)&signature);
}

void CodeGen::update_variable_value(const std::string& name, llvm::Value* value)
{
    for (auto it = scoped_symbol_table.end() - 1; it != scoped_symbol_table.begin(); it--)
    {
        if (it->contains(name)) {
            it->insert_or_assign(name, value);
            return;
        }
    }
    if (scoped_symbol_table.front().contains(name)) { // global variable
        auto global_variable = llvm::cast<llvm::GlobalVariable>(scoped_symbol_table.front().at(name));
        builder->CreateStore(value, global_variable);
    }
}

llvm::Value* CodeGen::find_variable_value(const std::string& name)
{
    for (auto it = scoped_symbol_table.end() - 1; it != scoped_symbol_table.begin(); it--)
    {
        if (it->contains(name)) return it->at(name);
    }
    if (scoped_symbol_table.front().contains(name)) { // global variable
        auto global_variable = llvm::cast<llvm::GlobalVariable>(scoped_symbol_table.front().at(name));
        return builder->CreateLoad(global_variable->getValueType(), global_variable);
    }
}

void CodeGen::release_lifecycle_resources(bool is_function_return, llvm::Value* return_value)
{
    while (lifecycles.size() > 0) {
        for (auto value : lifecycles.top().values) {
            if (value != return_value) {
                builder->CreateCall(module->getFunction("_ziyue4d_release_string__"), { value });
            }
        }
        if ((!is_function_return) || (is_function_return && lifecycles.top().is_function)) {
            lifecycles.pop();
            break;
        }
        else {
            lifecycles.pop();
        }
    }
}

llvm::Value* CodeGen::build_literal_string(const std::string& str)
{
    llvm::Value* built_string = builder->CreateCall(module->getFunction("_ziyue4d_create_string__"), { builder->CreateGlobalStringPtr(str) });
    lifecycles.top().values.insert(built_string);
    return built_string;
}

#include <llvm/Support/MemoryBuffer.h>
#include <llvm/IRReader/IRReader.h>
void JIT::init()
{
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();
    llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);
    auto jit = llvm::orc::LLJITBuilder().create();
    if (!jit) throw std::runtime_error("failed to initialize JIT");
    this->jit = std::move(*jit);
    this->jit->getMainJITDylib().addGenerator(
        llvm::cantFail(llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
            this->jit->getDataLayout().getGlobalPrefix()))
    );
    auto stdlib = llvm::parseBitcodeFile(**llvm::MemoryBuffer::getFile("stdlib.bc"), *context);
    auto std_module = llvm::orc::ThreadSafeModule(std::move(*stdlib), std::make_unique<llvm::LLVMContext>());
    auto program_module = llvm::orc::ThreadSafeModule(std::move(module), std::make_unique<llvm::LLVMContext>());
    this->jit->addIRModule(std::move(std_module));
    this->jit->addIRModule(std::move(program_module));
}

int JIT::run()
{
    auto sym = jit->lookup("__main");
    auto main = sym->toPtr<int (*)()>();
    int result = main();
    return result;
}
