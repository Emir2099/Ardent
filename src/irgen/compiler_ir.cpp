#ifdef ARDENT_ENABLE_LLVM
#include "compiler_ir.h"
#include "../types.h"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/GlobalVariable.h>

IRCompiler::IRCompiler(llvm::LLVMContext &c, llvm::Module &m, ArdentRuntime &rt)
    : ctx(c), mod(m), runtime(rt) {}

// ─── Type-Aware Fast Path Detection ────────────────────────────────────
// Ardent 2.2: Leverage type annotations and inference for LLVM fast paths.
// A node is "statically typed whole" if:
//   1. It has a declared :whole type rune, OR
//   2. It was inferred as whole during type inference, OR
//   3. It's a numeric literal or binary op on numeric literals (legacy path)

static bool hasWholeType(ASTNode* n) {
    if (!n) return false;
    
    // Check TypeAnnotation from 2.2 type system
    const auto& ti = n->typeInfo;
    if (ti.hasRune && ti.declaredType.kind == ardent::TypeKind::Whole) {
        return true;
    }
    if (ti.inferredType.kind == ardent::TypeKind::Whole) {
        return true;
    }
    
    return false;
}

static bool hasTruthType(ASTNode* n) {
    if (!n) return false;
    const auto& ti = n->typeInfo;
    if (ti.hasRune && ti.declaredType.kind == ardent::TypeKind::Truth) {
        return true;
    }
    if (ti.inferredType.kind == ardent::TypeKind::Truth) {
        return true;
    }
    return false;
}

// Legacy detection: detect purely numeric expressions composed of
// number literals and nested numeric binary ops (+,-,*,/).
static bool ardentIsStaticallyNumeric(ASTNode* n) {
    if (!n) return false;
    
    // Ardent 2.2: Check type annotations first (faster path)
    if (hasWholeType(n)) return true;
    
    if (auto *E = dynamic_cast<Expression*>(n)) {
        return E->token.type == TokenType::NUMBER; // only numeric literal
    }
    if (auto *B = dynamic_cast<BinaryExpression*>(n)) {
        if (B->op.type == TokenType::OPERATOR) {
            const std::string &v = B->op.value;
            if (v == "+" || v == "-" || v == "*" || v == "/") {
                return ardentIsStaticallyNumeric(B->left.get()) && ardentIsStaticallyNumeric(B->right.get());
            }
        }
    }
    return false;
}

llvm::Value* IRCompiler::compileExpr(ASTNode* node) {
    using namespace llvm;
    if (!node) return UndefValue::get(getArdentValueTy());

    // Ardent 3.2: Collection literals
    if (auto *A = dynamic_cast<ArrayLiteral*>(node)) {
        return compileArrayLiteral(A);
    }
    if (auto *O = dynamic_cast<ObjectLiteral*>(node)) {
        return compileObjectLiteral(O);
    }
    
    // Ardent 3.2: Index expression
    if (auto *I = dynamic_cast<IndexExpression*>(node)) {
        return compileIndexExpr(I);
    }
    
    // Ardent 3.2: Containment
    if (auto *C = dynamic_cast<ContainsExpr*>(node)) {
        return compileContains(C);
    }
    
    // Ardent 3.2: Where clause
    if (auto *W = dynamic_cast<WhereExpr*>(node)) {
        return compileWhereExpr(W);
    }
    
    // Ardent 3.2: Transform expression
    if (auto *T = dynamic_cast<TransformExpr*>(node)) {
        return compileTransformExpr(T);
    }

    // Literal or identifier
    if (auto *E = dynamic_cast<Expression*>(node)) {
        const Token &tok = E->token;
        switch (tok.type) {
            case TokenType::NUMBER: {
                int64_t v = 0;
                try { v = std::stoll(tok.value); } catch (...) { v = 0; }
                return makeNumber(v);
            }
            case TokenType::STRING:
                return makePhrase(tok.value);
            case TokenType::BOOLEAN: {
                bool b = (tok.value == "true" || tok.value == "True" || tok.value == "TRUE" || tok.value == "1");
                return makeBoolean(b);
            }
            case TokenType::IDENTIFIER: {
                auto it = namedValues.find(tok.value);
                if (it == namedValues.end()) {
                    return UndefValue::get(getArdentValueTy());
                }
                return builder.CreateLoad(getArdentValueTy(), it->second, tok.value);
            }
            default:
                return UndefValue::get(getArdentValueTy());
        }
    }

    // Spell invocation
    if (auto *SI = dynamic_cast<SpellInvocation*>(node)) {
        std::string fnName = std::string("spell_") + SI->spellName;
        // Ensure declaration exists with the correct arity
        llvm::Function *callee = getOrCreateSpellDecl(fnName, SI->args.size());
        std::vector<Value*> args;
        args.reserve(SI->args.size());
        for (auto &a : SI->args) args.push_back(compileExpr(a.get()));
        return builder.CreateCall(callee, args, SI->spellName + std::string(".ret"));
    }

    // Binary operations
    if (auto *B = dynamic_cast<BinaryExpression*>(node)) {
        Value *L = compileExpr(B->left.get());
        Value *R = compileExpr(B->right.get());
        Value *Li = extractNumber(L);
        Value *Ri = extractNumber(R);

        const Token &op = B->op;
        // Arithmetic via runtime shims
        if (op.type == TokenType::OPERATOR) {
            if (op.value == "+") {
                // Static numeric fast path: emit raw LLVM add
                if (ardentIsStaticallyNumeric(B->left.get()) && ardentIsStaticallyNumeric(B->right.get())) {
                    auto *sumRaw = builder.CreateAdd(Li, Ri, "add.fast");
                    return buildNumberFromI64(sumRaw);
                }
                // Dynamic '+' : numeric add if both tags == number else phrase concatenation via runtime helper
                auto *valTy = getArdentValueTy();
                // Extract tags
                Value *tagL = builder.CreateExtractValue(L, {0}, "tagL");
                Value *tagR = builder.CreateExtractValue(R, {0}, "tagR");
                auto *tagNumConst = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), 0);
                Value *isNumL = builder.CreateICmpEQ(tagL, tagNumConst, "isNumL");
                Value *isNumR = builder.CreateICmpEQ(tagR, tagNumConst, "isNumR");
                Value *bothNums = builder.CreateAnd(isNumL, isNumR, "bothNums");
                // Prepare blocks
                llvm::Function *curFn = currentFunction;
                llvm::BasicBlock *numBB = llvm::BasicBlock::Create(ctx, "plus.num", curFn);
                llvm::BasicBlock *concatBB = llvm::BasicBlock::Create(ctx, "plus.concat", curFn);
                llvm::BasicBlock *mergeBB = llvm::BasicBlock::Create(ctx, "plus.merge", curFn);
                builder.CreateCondBr(bothNums, numBB, concatBB);
                // Result slot (in entry) and branch codegen
                llvm::AllocaInst *resultSlot = createAllocaInEntry("plus.result");
                // Numeric path
                builder.SetInsertPoint(numBB);
                auto *addFn = getIntBinOpDecl("ardent_rt_add_i64");
                auto *callv = builder.CreateCall(addFn, {Li, Ri}, "addtmp");
                Value *numRes = buildNumberFromI64(callv);
                builder.CreateStore(numRes, resultSlot);
                builder.CreateBr(mergeBB);
                // Concat path
                builder.SetInsertPoint(concatBB);
                auto *concatFn = getConcatDecl();
                // Allocate temporaries for operands (entry allocas already exist if createAllocaInEntry used)
                llvm::AllocaInst *leftSlot = createAllocaInEntry("concat.left");
                llvm::AllocaInst *rightSlot = createAllocaInEntry("concat.right");
                builder.CreateStore(L, leftSlot);
                builder.CreateStore(R, rightSlot);
                // Out slot for concat (reuse resultSlot)
                builder.CreateCall(concatFn, { leftSlot, rightSlot, resultSlot });
                builder.CreateBr(mergeBB);
                // Merge
                builder.SetInsertPoint(mergeBB);
                Value *finalV = builder.CreateLoad(valTy, resultSlot, "plus.final");
                return finalV;
            } else if (op.value == "-") {
                if (ardentIsStaticallyNumeric(B->left.get()) && ardentIsStaticallyNumeric(B->right.get())) {
                    auto *diffRaw = builder.CreateSub(Li, Ri, "sub.fast");
                    return buildNumberFromI64(diffRaw);
                }
                auto *fn = getIntBinOpDecl("ardent_rt_sub_i64");
                auto *callv = builder.CreateCall(fn, {Li, Ri}, "subtmp");
                return buildNumberFromI64(callv);
            } else if (op.value == "*") {
                if (ardentIsStaticallyNumeric(B->left.get()) && ardentIsStaticallyNumeric(B->right.get())) {
                    auto *prodRaw = builder.CreateMul(Li, Ri, "mul.fast");
                    return buildNumberFromI64(prodRaw);
                }
                auto *fn = getIntBinOpDecl("ardent_rt_mul_i64");
                auto *callv = builder.CreateCall(fn, {Li, Ri}, "multmp");
                return buildNumberFromI64(callv);
            } else if (op.value == "/") {
                auto *fn = getIntBinOpDecl("ardent_rt_div_i64");
                auto *callv = builder.CreateCall(fn, {Li, Ri}, "divtmp");
                return buildNumberFromI64(callv);
            }
        }

        // Comparisons
        if (op.type == TokenType::EQUAL) {
            return buildBooleanFromI1(builder.CreateICmpEQ(Li, Ri, "cmptmp"));
        }
        if (op.type == TokenType::NOT_EQUAL) {
            return buildBooleanFromI1(builder.CreateICmpNE(Li, Ri, "cmptmp"));
        }
        if (op.type == TokenType::GREATER) {
            return buildBooleanFromI1(builder.CreateICmpSGT(Li, Ri, "cmptmp"));
        }
        if (op.type == TokenType::LESSER) {
            return buildBooleanFromI1(builder.CreateICmpSLT(Li, Ri, "cmptmp"));
        }

        return UndefValue::get(getArdentValueTy());
    }

    return UndefValue::get(getArdentValueTy());
}

void IRCompiler::compileStmt(ASTNode* node) {
    using namespace llvm;
    if (!node) return;

    // Ardent 3.2: For-each iteration
    if (auto *FE = dynamic_cast<ForEachStmt*>(node)) {
        compileForEach(FE);
        return;
    }
    
    // Ardent 3.2: Index assignment
    if (auto *IA = dynamic_cast<IndexAssignStmt*>(node)) {
        compileIndexAssign(IA);
        return;
    }

    // Typed variable declaration (2.2)
    if (auto *VD = dynamic_cast<VariableDeclaration*>(node)) {
        std::string name = VD->varName;
        auto *slot = getOrCreateVar(name);
        Value *R = compileExpr(VD->initializer.get());
        
        // Type-aware fast path: if declaredType is known, we can use typed storage
        // For now, store into the generic ArdentValue slot (future: typed alloca)
        builder.CreateStore(R, slot);
        return;
    }

    // Variable declaration / assignment: BinaryExpression with op IS_OF
    if (auto *B = dynamic_cast<BinaryExpression*>(node)) {
        if (B->op.type == TokenType::IS_OF) {
            auto *LExpr = dynamic_cast<Expression*>(B->left.get());
            if (LExpr && LExpr->token.type == TokenType::IDENTIFIER) {
                std::string name = LExpr->token.value;
                auto *slot = getOrCreateVar(name);
                Value *R = compileExpr(B->right.get());
                builder.CreateStore(R, slot);
                return;
            }
        }
    }

    // Print proclamation
    if (auto *P = dynamic_cast<PrintStatement*>(node)) {
        Value *V = compileExpr(P->expression.get());
        auto *printFn = getPrintDecl();
        // Store value to a stack slot and pass pointer to runtime
        AllocaInst *tmp = createAllocaInEntry("print.tmp");
        builder.CreateStore(V, tmp);
        (void)builder.CreateCall(printFn, { tmp });
        return;
    }

    // If/Else
    if (auto *I = dynamic_cast<IfStatement*>(node)) {
        if (!currentFunction) return;
        Value *condV = compileExpr(I->condition.get());
        Value *truth = extractBoolean(condV);
        BasicBlock *thenBB = BasicBlock::Create(ctx, "then", currentFunction);
        BasicBlock *elseBB = BasicBlock::Create(ctx, "else", currentFunction);
        BasicBlock *mergeBB = BasicBlock::Create(ctx, "ifend", currentFunction);
        if (I->elseBranch) {
            builder.CreateCondBr(truth, thenBB, elseBB);
        } else {
            builder.CreateCondBr(truth, thenBB, mergeBB);
        }
        // then
        builder.SetInsertPoint(thenBB);
        compileStmt(I->thenBranch.get());
        if (!thenBB->getTerminator()) builder.CreateBr(mergeBB);
        // else
        if (I->elseBranch) {
            builder.SetInsertPoint(elseBB);
            compileStmt(I->elseBranch.get());
            if (!elseBB->getTerminator()) builder.CreateBr(mergeBB);
        }
        // merge
        builder.SetInsertPoint(mergeBB);
        return;
    }

    // While loop (using WhileLoop AST)
    if (auto *W = dynamic_cast<WhileLoop*>(node)) {
        if (!currentFunction) return;
        BasicBlock *loopStart = BasicBlock::Create(ctx, "loop.start", currentFunction);
        BasicBlock *loopBody  = BasicBlock::Create(ctx, "loop.body", currentFunction);
        BasicBlock *loopEnd   = BasicBlock::Create(ctx, "loop.end", currentFunction);
        builder.CreateBr(loopStart);
        builder.SetInsertPoint(loopStart);

        // Build condition from loopVar and limit
        auto *loopVarExpr = dynamic_cast<Expression*>(W->loopVar.get());
        auto *limitExpr   = dynamic_cast<Expression*>(W->limit.get());
        Value *loopVarV = compileExpr(loopVarExpr);
        Value *limitV   = compileExpr(limitExpr);
        Value *Li = extractNumber(loopVarV);
        Value *Ri = extractNumber(limitV);
        Value *condI1 = nullptr;
        if (W->comparisonOp == TokenType::SURPASSETH) {
            condI1 = builder.CreateICmpSGT(Li, Ri, "whilegt");
        } else { // REMAINETH or default
            condI1 = builder.CreateICmpSLT(Li, Ri, "whilelt");
        }
        builder.CreateCondBr(condI1, loopBody, loopEnd);

        // Body
        builder.SetInsertPoint(loopBody);
        for (auto &stmt : W->body) compileStmt(stmt.get());
        // Update: let var ascend/descend step
        auto *stepExpr = dynamic_cast<Expression*>(W->step.get());
        Value *stepV   = compileExpr(stepExpr);
        Value *Si = extractNumber(stepV);
        // Reload and compute new value
        auto varIt = namedValues.find(loopVarExpr->token.value);
        if (varIt != namedValues.end()) {
            Value *curV = builder.CreateLoad(getArdentValueTy(), varIt->second, loopVarExpr->token.value);
            Value *curI = extractNumber(curV);
            const char* rtName = (W->stepDirection == TokenType::ASCEND) ? "ardent_rt_add_i64" : "ardent_rt_sub_i64";
            auto *rtFn = getIntBinOpDecl(rtName);
            Value *nextI = builder.CreateCall(rtFn, {curI, Si}, "step");
            Value *nextV = buildNumberFromI64(nextI);
            builder.CreateStore(nextV, varIt->second);
        }
        builder.CreateBr(loopStart);

        // End
        builder.SetInsertPoint(loopEnd);
        return;
    }

    // Block: sequence of statements
    if (auto *B = dynamic_cast<BlockStatement*>(node)) {
        for (auto &stmt : B->statements) compileStmt(stmt.get());
        return;
    }

    // Return statement
    if (auto *R = dynamic_cast<ReturnStatement*>(node)) {
        llvm::Value *retV = compileExpr(R->expression.get());
        builder.CreateRet(retV);
        return;
    }
}

void IRCompiler::compileSpell(SpellStatement* sp) {
    if (!sp) return;
    using namespace llvm;
    std::string fnName = std::string("spell_") + sp->spellName;
    // Create or fetch declaration first
    Function *fn = getOrCreateSpellDecl(fnName, sp->params.size());

    // Save/restore codegen state
    auto *prevFn = currentFunction;
    auto savedNamed = namedValues;

    // Entry block
    BasicBlock *entry = BasicBlock::Create(ctx, "entry", fn);
    builder.SetInsertPoint(entry);
    currentFunction = fn;
    namedValues.clear();

    // Allocate and store parameters into allocas, then bind
    size_t idx = 0;
    for (auto &arg : fn->args()) {
        std::string pname = (idx < sp->params.size() ? sp->params[idx] : ("arg" + std::to_string(idx)));
        arg.setName(pname);
        AllocaInst *slot = createAllocaInEntry(pname);
        builder.CreateStore(&arg, slot);
        bindAlloca(pname, slot);
        ++idx;
    }

    // Compile body
    compileStmt(sp->body.get());
    // Ensure return exists
    if (!entry->getParent()->back().getTerminator()) {
        // If the current insertion block has no terminator, emit default return
        builder.CreateRet(makeNumber(0));
    } else if (!builder.GetInsertBlock()->getTerminator()) {
        builder.CreateRet(makeNumber(0));
    }

    // Restore state
    currentFunction = prevFn;
    namedValues = std::move(savedNamed);
}

void IRCompiler::compileProgram(BlockStatement* root) {
    if (!root) return;
    using namespace llvm;
    // Pass 1: define all spells
    for (auto &s : root->statements) {
        if (auto sp = dynamic_cast<SpellStatement*>(s.get())) {
            compileSpell(sp);
        }
    }
    // Pass 2: create module-level entry to run top-level statements
    // int ardent_entry()
    auto *i32 = Type::getInt32Ty(ctx);
    auto *fty = FunctionType::get(i32, {}, false);
    Function *entryFn = Function::Create(fty, Function::ExternalLinkage, "ardent_entry", mod);
    BasicBlock *bb = BasicBlock::Create(ctx, "entry", entryFn);
    auto *prevFn = currentFunction;
    auto savedNamed = namedValues;
    builder.SetInsertPoint(bb);
    currentFunction = entryFn;
    namedValues.clear();
    for (auto &s : root->statements) {
        if (!dynamic_cast<SpellStatement*>(s.get())) compileStmt(s.get());
    }
    // Ensure function returns even if entry had an initial branch
    if (!builder.GetInsertBlock()->getTerminator()) {
        builder.CreateRet(ConstantInt::get(i32, 0));
    }
    currentFunction = prevFn;
    namedValues = std::move(savedNamed);
}

// ===== PHASE 2: ArdentValue ABI in IR =====
llvm::StructType* IRCompiler::getArdentValueTy() {
    if (ardentValueTy) return ardentValueTy;
    using namespace llvm;
    auto &C = ctx;
    auto i1  = Type::getInt1Ty(C);
    auto i8  = Type::getInt8Ty(C);
    auto i32 = Type::getInt32Ty(C);
    auto i64 = Type::getInt64Ty(C);
    auto i8p = llvm::PointerType::get(Type::getInt8Ty(C), 0);
    ardentValueTy = StructType::create(C, "ArdentValue");
    // Use i8 for 'truth' to match C ABI (bool is 1 byte)
    std::vector<Type*> fields = { i32, i64, i8, i8p, i32 };
    ardentValueTy->setBody(fields, /*isPacked*/false);
    return ardentValueTy;
}

llvm::Value* IRCompiler::makeNumber(int64_t v) {
    using namespace llvm;
    auto *ty = getArdentValueTy();
    auto &C = ctx;
    Constant *tag   = ConstantInt::get(Type::getInt32Ty(C), 0); // number tag
    Constant *num   = ConstantInt::get(Type::getInt64Ty(C), v);
    Constant *truth = ConstantInt::get(Type::getInt8Ty(C), 0);
    Constant *str   = ConstantPointerNull::get(llvm::PointerType::get(Type::getInt8Ty(C), 0));
    Constant *len   = ConstantInt::get(Type::getInt32Ty(C), 0);
    return ConstantStruct::get(ty, { tag, num, truth, str, len });
}

llvm::Value* IRCompiler::makeBoolean(bool b) {
    using namespace llvm;
    auto *ty = getArdentValueTy();
    auto &C = ctx;
    Constant *tag   = ConstantInt::get(Type::getInt32Ty(C), 2); // truth tag
    Constant *num   = ConstantInt::get(Type::getInt64Ty(C), 0);
    Constant *truth = ConstantInt::get(Type::getInt8Ty(C), b ? 1 : 0);
    Constant *str   = ConstantPointerNull::get(llvm::PointerType::get(Type::getInt8Ty(C), 0));
    Constant *len   = ConstantInt::get(Type::getInt32Ty(C), 0);
    return ConstantStruct::get(ty, { tag, num, truth, str, len });
}

llvm::Value* IRCompiler::makePhrase(const std::string &s) {
    using namespace llvm;
    auto *ty = getArdentValueTy();
    auto &C = ctx;
    // Create/emit a private global for the string bytes (with NUL)
    auto arr = ConstantDataArray::getString(C, s, true);
    auto arrTy = cast<ArrayType>(arr->getType());
    std::string gname = std::string(".ardent.str.") + std::to_string(nextStringId++);
    auto gv = new GlobalVariable(mod, arrTy, /*isConstant*/true, GlobalValue::PrivateLinkage, arr, gname);
    gv->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
    gv->setAlignment(llvm::MaybeAlign(1));
    Constant *zero32 = ConstantInt::get(Type::getInt32Ty(C), 0);
    Constant *indices[] = { zero32, zero32 };
    Constant *ptr = ConstantExpr::getInBoundsGetElementPtr(arrTy, gv, indices);

    Constant *tag   = ConstantInt::get(Type::getInt32Ty(C), 1); // phrase tag
    Constant *num   = ConstantInt::get(Type::getInt64Ty(C), 0);
    Constant *truth = ConstantInt::get(Type::getInt8Ty(C), 0);
    Constant *len   = ConstantInt::get(Type::getInt32Ty(C), static_cast<uint32_t>(s.size()));
    return ConstantStruct::get(ty, { tag, num, truth, ptr, len });
}

// ----- Statement helpers -----
llvm::AllocaInst* IRCompiler::createAllocaInEntry(const std::string &name) {
    using namespace llvm;
    if (!currentFunction) return nullptr;
    IRBuilder<> tmp(&currentFunction->getEntryBlock(), currentFunction->getEntryBlock().begin());
    auto *alloca = tmp.CreateAlloca(getArdentValueTy(), nullptr, name);
    return alloca;
}

llvm::AllocaInst* IRCompiler::getOrCreateVar(const std::string &name) {
    auto it = namedValues.find(name);
    if (it != namedValues.end()) return it->second;
    auto *slot = createAllocaInEntry(name);
    namedValues[name] = slot;
    return slot;
}

llvm::Value* IRCompiler::extractBoolean(llvm::Value* ardentV) {
    // Field 2 is i8; convert to i1 by comparing != 0
    auto *i8Val = builder.CreateExtractValue(ardentV, {2}, "truth8");
    auto *zero = llvm::ConstantInt::get(llvm::Type::getInt8Ty(ctx), 0);
    return builder.CreateICmpNE(i8Val, zero, "truth");
}

llvm::Function* IRCompiler::getPrintDecl() {
    using namespace llvm;
    // void ardent_rt_print_av_ptr(ArdentValue*)
    auto *valTy = getArdentValueTy();
    auto *ptrTy = llvm::PointerType::get(valTy, 0);
    if (auto *fn = mod.getFunction("ardent_rt_print_av_ptr")) return fn;
    auto *fty = FunctionType::get(Type::getVoidTy(ctx), {ptrTy}, false);
    auto *fn = Function::Create(fty, Function::ExternalLinkage, "ardent_rt_print_av_ptr", mod);
    return fn;
}

llvm::Function* IRCompiler::getConcatDecl() {
    using namespace llvm;
    auto *valTy = getArdentValueTy();
    auto *ptrTy = llvm::PointerType::get(valTy, 0);
    if (auto *fn = mod.getFunction("ardent_rt_concat_av_ptr")) return fn;
    // void ardent_rt_concat_av_ptr(ArdentValue*, ArdentValue*, ArdentValue*)
    auto *fty = FunctionType::get(Type::getVoidTy(ctx), {ptrTy, ptrTy, ptrTy}, false);
    auto *fn = Function::Create(fty, Function::ExternalLinkage, "ardent_rt_concat_av_ptr", mod);
    return fn;
}

llvm::Function* IRCompiler::getOrCreateSpellDecl(const std::string &name, size_t paramCount) {
    using namespace llvm;
    if (auto *fn = mod.getFunction(name)) return fn;
    auto *valTy = getArdentValueTy();
    std::vector<Type*> params(paramCount, valTy);
    auto *fty = FunctionType::get(valTy, params, false);
    auto *fn = Function::Create(fty, Function::ExternalLinkage, name, mod);
    // Give default arg names
    size_t idx = 0; for (auto &arg : fn->args()) { arg.setName("arg" + std::to_string(idx++)); }
    return fn;
}

// ===== PHASE 3: Literal expressions, identifiers, binary ops =====
llvm::Function* IRCompiler::getIntBinOpDecl(const char* name) {
    using namespace llvm;
    auto *fn = mod.getFunction(name);
    if (fn) return fn;
    auto i64 = Type::getInt64Ty(ctx);
    auto fty = FunctionType::get(i64, {i64, i64}, false);
    fn = Function::Create(fty, Function::ExternalLinkage, name, mod);
    return fn;
}

llvm::Value* IRCompiler::extractNumber(llvm::Value* ardentV) {
    // Field 1: i64 valueNum
    return builder.CreateExtractValue(ardentV, {1}, "num");
}

llvm::Value* IRCompiler::buildNumberFromI64(llvm::Value* v) {
    using namespace llvm;
    auto *ty = getArdentValueTy();
    Value *undef = UndefValue::get(ty);
    undef = builder.CreateInsertValue(undef, ConstantInt::get(Type::getInt32Ty(ctx), 0), {0});
    undef = builder.CreateInsertValue(undef, v, {1});
    undef = builder.CreateInsertValue(undef, ConstantInt::get(llvm::Type::getInt8Ty(ctx), 0), {2});
    undef = builder.CreateInsertValue(undef, ConstantPointerNull::get(PointerType::get(Type::getInt8Ty(ctx), 0)), {3});
    undef = builder.CreateInsertValue(undef, ConstantInt::get(Type::getInt32Ty(ctx), 0), {4});
    return undef;
}

llvm::Value* IRCompiler::buildBooleanFromI1(llvm::Value* v) {
    using namespace llvm;
    auto *ty = getArdentValueTy();
    Value *undef = UndefValue::get(ty);
    undef = builder.CreateInsertValue(undef, ConstantInt::get(Type::getInt32Ty(ctx), 2), {0});
    undef = builder.CreateInsertValue(undef, ConstantInt::get(Type::getInt64Ty(ctx), 0), {1});
    // Store boolean as i8 in field 2
    auto *v8 = builder.CreateZExt(v, llvm::Type::getInt8Ty(ctx));
    undef = builder.CreateInsertValue(undef, v8, {2});
    undef = builder.CreateInsertValue(undef, ConstantPointerNull::get(PointerType::get(Type::getInt8Ty(ctx), 0)), {3});
    undef = builder.CreateInsertValue(undef, ConstantInt::get(Type::getInt32Ty(ctx), 0), {4});
    return undef;
}

// =============================================================================
// Ardent 3.2: Collection Operations LLVM Lowering
// =============================================================================

// ----- Collection Runtime Declarations -----

llvm::Function* IRCompiler::getOrderNewDecl() {
    using namespace llvm;
    if (auto *fn = mod.getFunction("ardent_order_new")) return fn;
    // void* ardent_order_new()
    auto *voidPtrTy = PointerType::get(Type::getInt8Ty(ctx), 0);
    auto *fty = FunctionType::get(voidPtrTy, {}, false);
    return Function::Create(fty, Function::ExternalLinkage, "ardent_order_new", mod);
}

llvm::Function* IRCompiler::getOrderPushDecl() {
    using namespace llvm;
    if (auto *fn = mod.getFunction("ardent_order_push")) return fn;
    // void ardent_order_push(void* order, ArdentValue* val)
    auto *voidPtrTy = PointerType::get(Type::getInt8Ty(ctx), 0);
    auto *valPtrTy = PointerType::get(getArdentValueTy(), 0);
    auto *fty = FunctionType::get(Type::getVoidTy(ctx), {voidPtrTy, valPtrTy}, false);
    return Function::Create(fty, Function::ExternalLinkage, "ardent_order_push", mod);
}

llvm::Function* IRCompiler::getOrderGetDecl() {
    using namespace llvm;
    if (auto *fn = mod.getFunction("ardent_order_get")) return fn;
    // void ardent_order_get(void* order, int64_t idx, ArdentValue* out)
    auto *voidPtrTy = PointerType::get(Type::getInt8Ty(ctx), 0);
    auto *i64 = Type::getInt64Ty(ctx);
    auto *valPtrTy = PointerType::get(getArdentValueTy(), 0);
    auto *fty = FunctionType::get(Type::getVoidTy(ctx), {voidPtrTy, i64, valPtrTy}, false);
    return Function::Create(fty, Function::ExternalLinkage, "ardent_order_get", mod);
}

llvm::Function* IRCompiler::getOrderSetDecl() {
    using namespace llvm;
    if (auto *fn = mod.getFunction("ardent_order_set")) return fn;
    // void ardent_order_set(void* order, int64_t idx, ArdentValue* val)
    auto *voidPtrTy = PointerType::get(Type::getInt8Ty(ctx), 0);
    auto *i64 = Type::getInt64Ty(ctx);
    auto *valPtrTy = PointerType::get(getArdentValueTy(), 0);
    auto *fty = FunctionType::get(Type::getVoidTy(ctx), {voidPtrTy, i64, valPtrTy}, false);
    return Function::Create(fty, Function::ExternalLinkage, "ardent_order_set", mod);
}

llvm::Function* IRCompiler::getOrderLenDecl() {
    using namespace llvm;
    if (auto *fn = mod.getFunction("ardent_order_len")) return fn;
    // int64_t ardent_order_len(void* order)
    auto *voidPtrTy = PointerType::get(Type::getInt8Ty(ctx), 0);
    auto *i64 = Type::getInt64Ty(ctx);
    auto *fty = FunctionType::get(i64, {voidPtrTy}, false);
    return Function::Create(fty, Function::ExternalLinkage, "ardent_order_len", mod);
}

llvm::Function* IRCompiler::getTomeNewDecl() {
    using namespace llvm;
    if (auto *fn = mod.getFunction("ardent_tome_new")) return fn;
    // void* ardent_tome_new()
    auto *voidPtrTy = PointerType::get(Type::getInt8Ty(ctx), 0);
    auto *fty = FunctionType::get(voidPtrTy, {}, false);
    return Function::Create(fty, Function::ExternalLinkage, "ardent_tome_new", mod);
}

llvm::Function* IRCompiler::getTomeGetDecl() {
    using namespace llvm;
    if (auto *fn = mod.getFunction("ardent_tome_get")) return fn;
    // void ardent_tome_get(void* tome, const char* key, ArdentValue* out)
    auto *voidPtrTy = PointerType::get(Type::getInt8Ty(ctx), 0);
    auto *i8PtrTy = PointerType::get(Type::getInt8Ty(ctx), 0);
    auto *valPtrTy = PointerType::get(getArdentValueTy(), 0);
    auto *fty = FunctionType::get(Type::getVoidTy(ctx), {voidPtrTy, i8PtrTy, valPtrTy}, false);
    return Function::Create(fty, Function::ExternalLinkage, "ardent_tome_get", mod);
}

llvm::Function* IRCompiler::getTomeSetDecl() {
    using namespace llvm;
    if (auto *fn = mod.getFunction("ardent_tome_set")) return fn;
    // void ardent_tome_set(void* tome, const char* key, ArdentValue* val)
    auto *voidPtrTy = PointerType::get(Type::getInt8Ty(ctx), 0);
    auto *i8PtrTy = PointerType::get(Type::getInt8Ty(ctx), 0);
    auto *valPtrTy = PointerType::get(getArdentValueTy(), 0);
    auto *fty = FunctionType::get(Type::getVoidTy(ctx), {voidPtrTy, i8PtrTy, valPtrTy}, false);
    return Function::Create(fty, Function::ExternalLinkage, "ardent_tome_set", mod);
}

llvm::Function* IRCompiler::getContainsDecl() {
    using namespace llvm;
    if (auto *fn = mod.getFunction("ardent_contains")) return fn;
    // bool ardent_contains(ArdentValue* collection, ArdentValue* needle)
    auto *valPtrTy = PointerType::get(getArdentValueTy(), 0);
    auto *i1 = Type::getInt1Ty(ctx);
    auto *fty = FunctionType::get(i1, {valPtrTy, valPtrTy}, false);
    return Function::Create(fty, Function::ExternalLinkage, "ardent_contains", mod);
}

llvm::Function* IRCompiler::getIterOrderDecl() {
    using namespace llvm;
    if (auto *fn = mod.getFunction("ardent_iter_order")) return fn;
    // void* ardent_iter_order(void* order)
    auto *voidPtrTy = PointerType::get(Type::getInt8Ty(ctx), 0);
    auto *fty = FunctionType::get(voidPtrTy, {voidPtrTy}, false);
    return Function::Create(fty, Function::ExternalLinkage, "ardent_iter_order", mod);
}

llvm::Function* IRCompiler::getIterTomeKVDecl() {
    using namespace llvm;
    if (auto *fn = mod.getFunction("ardent_iter_tome_kv")) return fn;
    // void* ardent_iter_tome_kv(void* tome)
    auto *voidPtrTy = PointerType::get(Type::getInt8Ty(ctx), 0);
    auto *fty = FunctionType::get(voidPtrTy, {voidPtrTy}, false);
    return Function::Create(fty, Function::ExternalLinkage, "ardent_iter_tome_kv", mod);
}

llvm::Function* IRCompiler::getIterHasNextDecl() {
    using namespace llvm;
    if (auto *fn = mod.getFunction("ardent_iter_has_next")) return fn;
    // bool ardent_iter_has_next(void* iter)
    auto *voidPtrTy = PointerType::get(Type::getInt8Ty(ctx), 0);
    auto *i1 = Type::getInt1Ty(ctx);
    auto *fty = FunctionType::get(i1, {voidPtrTy}, false);
    return Function::Create(fty, Function::ExternalLinkage, "ardent_iter_has_next", mod);
}

llvm::Function* IRCompiler::getIterNextDecl() {
    using namespace llvm;
    if (auto *fn = mod.getFunction("ardent_iter_next")) return fn;
    // void ardent_iter_next(void* iter, ArdentValue* out)
    auto *voidPtrTy = PointerType::get(Type::getInt8Ty(ctx), 0);
    auto *valPtrTy = PointerType::get(getArdentValueTy(), 0);
    auto *fty = FunctionType::get(Type::getVoidTy(ctx), {voidPtrTy, valPtrTy}, false);
    return Function::Create(fty, Function::ExternalLinkage, "ardent_iter_next", mod);
}

llvm::Function* IRCompiler::getIterNextKVDecl() {
    using namespace llvm;
    if (auto *fn = mod.getFunction("ardent_iter_next_kv")) return fn;
    // void ardent_iter_next_kv(void* iter, ArdentValue* keyOut, ArdentValue* valOut)
    auto *voidPtrTy = PointerType::get(Type::getInt8Ty(ctx), 0);
    auto *valPtrTy = PointerType::get(getArdentValueTy(), 0);
    auto *fty = FunctionType::get(Type::getVoidTy(ctx), {voidPtrTy, valPtrTy, valPtrTy}, false);
    return Function::Create(fty, Function::ExternalLinkage, "ardent_iter_next_kv", mod);
}

llvm::Function* IRCompiler::getIterFreeDecl() {
    using namespace llvm;
    if (auto *fn = mod.getFunction("ardent_iter_free")) return fn;
    // void ardent_iter_free(void* iter)
    auto *voidPtrTy = PointerType::get(Type::getInt8Ty(ctx), 0);
    auto *fty = FunctionType::get(Type::getVoidTy(ctx), {voidPtrTy}, false);
    return Function::Create(fty, Function::ExternalLinkage, "ardent_iter_free", mod);
}

llvm::Function* IRCompiler::getMakeOrderDecl() {
    using namespace llvm;
    if (auto *fn = mod.getFunction("ardent_make_order")) return fn;
    // void ardent_make_order(void* orderPtr, ArdentValue* out)
    auto *voidPtrTy = PointerType::get(Type::getInt8Ty(ctx), 0);
    auto *valPtrTy = PointerType::get(getArdentValueTy(), 0);
    auto *fty = FunctionType::get(Type::getVoidTy(ctx), {voidPtrTy, valPtrTy}, false);
    return Function::Create(fty, Function::ExternalLinkage, "ardent_make_order", mod);
}

llvm::Function* IRCompiler::getMakeTomeDecl() {
    using namespace llvm;
    if (auto *fn = mod.getFunction("ardent_make_tome")) return fn;
    // void ardent_make_tome(void* tomePtr, ArdentValue* out)
    auto *voidPtrTy = PointerType::get(Type::getInt8Ty(ctx), 0);
    auto *valPtrTy = PointerType::get(getArdentValueTy(), 0);
    auto *fty = FunctionType::get(Type::getVoidTy(ctx), {voidPtrTy, valPtrTy}, false);
    return Function::Create(fty, Function::ExternalLinkage, "ardent_make_tome", mod);
}

llvm::Function* IRCompiler::getExtractOrderDecl() {
    using namespace llvm;
    if (auto *fn = mod.getFunction("ardent_extract_order")) return fn;
    // void* ardent_extract_order(ArdentValue* val)
    auto *voidPtrTy = PointerType::get(Type::getInt8Ty(ctx), 0);
    auto *valPtrTy = PointerType::get(getArdentValueTy(), 0);
    auto *fty = FunctionType::get(voidPtrTy, {valPtrTy}, false);
    return Function::Create(fty, Function::ExternalLinkage, "ardent_extract_order", mod);
}

llvm::Function* IRCompiler::getExtractTomeDecl() {
    using namespace llvm;
    if (auto *fn = mod.getFunction("ardent_extract_tome")) return fn;
    // void* ardent_extract_tome(ArdentValue* val)
    auto *voidPtrTy = PointerType::get(Type::getInt8Ty(ctx), 0);
    auto *valPtrTy = PointerType::get(getArdentValueTy(), 0);
    auto *fty = FunctionType::get(voidPtrTy, {valPtrTy}, false);
    return Function::Create(fty, Function::ExternalLinkage, "ardent_extract_tome", mod);
}

// ----- Collection Compilation -----

llvm::Value* IRCompiler::makeOrder(const std::vector<llvm::Value*>& elements) {
    using namespace llvm;
    // Create order, push all elements, wrap in ArdentValue
    auto *orderPtr = builder.CreateCall(getOrderNewDecl(), {}, "order.new");
    for (auto *elem : elements) {
        auto *slot = createAllocaInEntry("order.elem");
        builder.CreateStore(elem, slot);
        builder.CreateCall(getOrderPushDecl(), {orderPtr, slot});
    }
    auto *resultSlot = createAllocaInEntry("order.result");
    builder.CreateCall(getMakeOrderDecl(), {orderPtr, resultSlot});
    return builder.CreateLoad(getArdentValueTy(), resultSlot, "order.val");
}

llvm::Value* IRCompiler::compileArrayLiteral(ArrayLiteral* arr) {
    if (!arr) return llvm::UndefValue::get(getArdentValueTy());
    std::vector<llvm::Value*> elements;
    elements.reserve(arr->elements.size());
    for (auto &e : arr->elements) {
        elements.push_back(compileExpr(e.get()));
    }
    return makeOrder(elements);
}

llvm::Value* IRCompiler::makeTome(const std::vector<std::pair<std::string, llvm::Value*>>& entries) {
    using namespace llvm;
    auto *tomePtr = builder.CreateCall(getTomeNewDecl(), {}, "tome.new");
    for (const auto &[key, val] : entries) {
        // Create string constant for key
        auto keyArr = ConstantDataArray::getString(ctx, key, true);
        auto arrTy = cast<ArrayType>(keyArr->getType());
        std::string gname = std::string(".tome.key.") + std::to_string(nextStringId++);
        auto *gv = new GlobalVariable(mod, arrTy, true, GlobalValue::PrivateLinkage, keyArr, gname);
        Constant *zero32 = ConstantInt::get(Type::getInt32Ty(ctx), 0);
        Constant *indices[] = { zero32, zero32 };
        Constant *keyPtr = ConstantExpr::getInBoundsGetElementPtr(arrTy, gv, indices);
        
        auto *valSlot = createAllocaInEntry("tome.val");
        builder.CreateStore(val, valSlot);
        builder.CreateCall(getTomeSetDecl(), {tomePtr, keyPtr, valSlot});
    }
    auto *resultSlot = createAllocaInEntry("tome.result");
    builder.CreateCall(getMakeTomeDecl(), {tomePtr, resultSlot});
    return builder.CreateLoad(getArdentValueTy(), resultSlot, "tome.val");
}

llvm::Value* IRCompiler::compileObjectLiteral(ObjectLiteral* obj) {
    if (!obj) return llvm::UndefValue::get(getArdentValueTy());
    std::vector<std::pair<std::string, llvm::Value*>> entries;
    entries.reserve(obj->entries.size());
    for (auto &[key, valNode] : obj->entries) {
        entries.emplace_back(key, compileExpr(valNode.get()));
    }
    return makeTome(entries);
}

llvm::Value* IRCompiler::compileIndexExpr(IndexExpression* idx) {
    using namespace llvm;
    if (!idx) return UndefValue::get(getArdentValueTy());
    
    Value *collVal = compileExpr(idx->object.get());
    Value *indexVal = compileExpr(idx->index.get());
    
    auto *collSlot = createAllocaInEntry("idx.coll");
    builder.CreateStore(collVal, collSlot);
    
    // Check tag to determine if it's an Order (tag=3) or Tome (tag=1 for phrase key)
    Value *tag = builder.CreateExtractValue(collVal, {0}, "tag");
    auto *tagOrderConst = ConstantInt::get(Type::getInt32Ty(ctx), 3);
    Value *isOrder = builder.CreateICmpEQ(tag, tagOrderConst, "isOrder");
    
    auto *curFn = currentFunction;
    auto *orderBB = BasicBlock::Create(ctx, "idx.order", curFn);
    auto *tomeBB = BasicBlock::Create(ctx, "idx.tome", curFn);
    auto *mergeBB = BasicBlock::Create(ctx, "idx.merge", curFn);
    
    auto *resultSlot = createAllocaInEntry("idx.result");
    builder.CreateCondBr(isOrder, orderBB, tomeBB);
    
    // Order path: use numeric index
    builder.SetInsertPoint(orderBB);
    Value *orderPtr = builder.CreateExtractValue(collVal, {3}, "orderPtr"); // coll field is i8*
    auto *orderPtrVoid = builder.CreateBitCast(orderPtr, PointerType::get(Type::getInt8Ty(ctx), 0));
    Value *numIdx = extractNumber(indexVal);
    builder.CreateCall(getOrderGetDecl(), {orderPtrVoid, numIdx, resultSlot});
    builder.CreateBr(mergeBB);
    
    // Tome path: use phrase key
    builder.SetInsertPoint(tomeBB);
    Value *tomePtr = builder.CreateExtractValue(collVal, {3}, "tomePtr");
    auto *tomePtrVoid = builder.CreateBitCast(tomePtr, PointerType::get(Type::getInt8Ty(ctx), 0));
    Value *keyPtr = builder.CreateExtractValue(indexVal, {3}, "keyStr");
    builder.CreateCall(getTomeGetDecl(), {tomePtrVoid, keyPtr, resultSlot});
    builder.CreateBr(mergeBB);
    
    builder.SetInsertPoint(mergeBB);
    return builder.CreateLoad(getArdentValueTy(), resultSlot, "idx.val");
}

void IRCompiler::compileIndexAssign(IndexAssignStmt* stmt) {
    using namespace llvm;
    if (!stmt) return;
    
    Value *collVal = compileExpr(stmt->collection.get());
    Value *indexVal = compileExpr(stmt->index.get());
    Value *newVal = compileExpr(stmt->value.get());
    
    Value *tag = builder.CreateExtractValue(collVal, {0}, "tag");
    auto *tagOrderConst = ConstantInt::get(Type::getInt32Ty(ctx), 3);
    Value *isOrder = builder.CreateICmpEQ(tag, tagOrderConst, "isOrder");
    
    auto *curFn = currentFunction;
    auto *orderBB = BasicBlock::Create(ctx, "assign.order", curFn);
    auto *tomeBB = BasicBlock::Create(ctx, "assign.tome", curFn);
    auto *mergeBB = BasicBlock::Create(ctx, "assign.merge", curFn);
    
    auto *valSlot = createAllocaInEntry("assign.val");
    builder.CreateStore(newVal, valSlot);
    builder.CreateCondBr(isOrder, orderBB, tomeBB);
    
    // Order path
    builder.SetInsertPoint(orderBB);
    Value *orderPtr = builder.CreateExtractValue(collVal, {3}, "orderPtr");
    auto *orderPtrVoid = builder.CreateBitCast(orderPtr, PointerType::get(Type::getInt8Ty(ctx), 0));
    Value *numIdx = extractNumber(indexVal);
    builder.CreateCall(getOrderSetDecl(), {orderPtrVoid, numIdx, valSlot});
    builder.CreateBr(mergeBB);
    
    // Tome path
    builder.SetInsertPoint(tomeBB);
    Value *tomePtr = builder.CreateExtractValue(collVal, {3}, "tomePtr");
    auto *tomePtrVoid = builder.CreateBitCast(tomePtr, PointerType::get(Type::getInt8Ty(ctx), 0));
    Value *keyPtr = builder.CreateExtractValue(indexVal, {3}, "keyStr");
    builder.CreateCall(getTomeSetDecl(), {tomePtrVoid, keyPtr, valSlot});
    builder.CreateBr(mergeBB);
    
    builder.SetInsertPoint(mergeBB);
}

llvm::Value* IRCompiler::compileContains(ContainsExpr* contains) {
    using namespace llvm;
    if (!contains) return makeBoolean(false);
    
    Value *needleVal = compileExpr(contains->needle.get());
    Value *collVal = compileExpr(contains->collection.get());
    
    auto *needleSlot = createAllocaInEntry("contains.needle");
    auto *collSlot = createAllocaInEntry("contains.coll");
    builder.CreateStore(needleVal, needleSlot);
    builder.CreateStore(collVal, collSlot);
    
    Value *result = builder.CreateCall(getContainsDecl(), {collSlot, needleSlot}, "contains.res");
    return buildBooleanFromI1(result);
}

void IRCompiler::compileForEach(ForEachStmt* forEach) {
    using namespace llvm;
    if (!forEach || !currentFunction) return;
    
    Value *collVal = compileExpr(forEach->collection.get());
    auto *collSlot = createAllocaInEntry("foreach.coll");
    builder.CreateStore(collVal, collSlot);
    
    // Check if it's an Order or Tome
    Value *tag = builder.CreateExtractValue(collVal, {0}, "tag");
    bool hasKeyVar = !forEach->keyVar.empty();
    
    // Create iterator
    Value *collPtr = builder.CreateExtractValue(collVal, {3}, "collPtr");
    auto *collPtrVoid = builder.CreateBitCast(collPtr, PointerType::get(Type::getInt8Ty(ctx), 0));
    
    Value *iter;
    if (hasKeyVar) {
        iter = builder.CreateCall(getIterTomeKVDecl(), {collPtrVoid}, "iter.kv");
    } else {
        iter = builder.CreateCall(getIterOrderDecl(), {collPtrVoid}, "iter");
    }
    
    // Allocate iteration variables
    auto *valSlot = getOrCreateVar(forEach->valueVar);
    llvm::AllocaInst *keySlot = nullptr;
    if (hasKeyVar) {
        keySlot = getOrCreateVar(forEach->keyVar);
    }
    
    // Loop structure
    auto *loopCond = BasicBlock::Create(ctx, "foreach.cond", currentFunction);
    auto *loopBody = BasicBlock::Create(ctx, "foreach.body", currentFunction);
    auto *loopEnd = BasicBlock::Create(ctx, "foreach.end", currentFunction);
    
    builder.CreateBr(loopCond);
    
    // Condition: check if iterator has more elements
    builder.SetInsertPoint(loopCond);
    Value *hasNext = builder.CreateCall(getIterHasNextDecl(), {iter}, "hasNext");
    builder.CreateCondBr(hasNext, loopBody, loopEnd);
    
    // Body: get next element(s) and execute body
    builder.SetInsertPoint(loopBody);
    if (hasKeyVar) {
        builder.CreateCall(getIterNextKVDecl(), {iter, keySlot, valSlot});
    } else {
        builder.CreateCall(getIterNextDecl(), {iter, valSlot});
    }
    
    // Compile loop body statements
    compileStmt(forEach->body.get());
    
    // Back to condition (if no terminator yet)
    if (!builder.GetInsertBlock()->getTerminator()) {
        builder.CreateBr(loopCond);
    }
    
    // End
    builder.SetInsertPoint(loopEnd);
    builder.CreateCall(getIterFreeDecl(), {iter});
}

llvm::Value* IRCompiler::compileWhereExpr(WhereExpr* where) {
    using namespace llvm;
    if (!where || !currentFunction) return UndefValue::get(getArdentValueTy());
    
    Value *collVal = compileExpr(where->collection.get());
    Value *collPtr = builder.CreateExtractValue(collVal, {3}, "collPtr");
    auto *collPtrVoid = builder.CreateBitCast(collPtr, PointerType::get(Type::getInt8Ty(ctx), 0));
    
    // Create result order
    auto *resultOrderPtr = builder.CreateCall(getOrderNewDecl(), {}, "where.result");
    
    // Create iterator
    auto *iter = builder.CreateCall(getIterOrderDecl(), {collPtrVoid}, "where.iter");
    
    // Create iteration variable slot
    auto *elemSlot = getOrCreateVar(where->itemVar);
    
    auto *loopCond = BasicBlock::Create(ctx, "where.cond", currentFunction);
    auto *loopBody = BasicBlock::Create(ctx, "where.body", currentFunction);
    auto *pushBB = BasicBlock::Create(ctx, "where.push", currentFunction);
    auto *contBB = BasicBlock::Create(ctx, "where.cont", currentFunction);
    auto *loopEnd = BasicBlock::Create(ctx, "where.end", currentFunction);
    
    builder.CreateBr(loopCond);
    
    builder.SetInsertPoint(loopCond);
    Value *hasNext = builder.CreateCall(getIterHasNextDecl(), {iter}, "hasNext");
    builder.CreateCondBr(hasNext, loopBody, loopEnd);
    
    builder.SetInsertPoint(loopBody);
    builder.CreateCall(getIterNextDecl(), {iter, elemSlot});
    
    // Compile predicate
    Value *predResult = compileExpr(where->predicate.get());
    Value *predBool = extractBoolean(predResult);
    builder.CreateCondBr(predBool, pushBB, contBB);
    
    builder.SetInsertPoint(pushBB);
    builder.CreateCall(getOrderPushDecl(), {resultOrderPtr, elemSlot});
    builder.CreateBr(contBB);
    
    builder.SetInsertPoint(contBB);
    builder.CreateBr(loopCond);
    
    builder.SetInsertPoint(loopEnd);
    builder.CreateCall(getIterFreeDecl(), {iter});
    
    // Wrap result order into ArdentValue
    auto *resultSlot = createAllocaInEntry("where.out");
    builder.CreateCall(getMakeOrderDecl(), {resultOrderPtr, resultSlot});
    return builder.CreateLoad(getArdentValueTy(), resultSlot, "where.val");
}

llvm::Value* IRCompiler::compileTransformExpr(TransformExpr* transform) {
    using namespace llvm;
    if (!transform || !currentFunction) return UndefValue::get(getArdentValueTy());
    
    Value *collVal = compileExpr(transform->collection.get());
    Value *collPtr = builder.CreateExtractValue(collVal, {3}, "collPtr");
    auto *collPtrVoid = builder.CreateBitCast(collPtr, PointerType::get(Type::getInt8Ty(ctx), 0));
    
    // Create result order
    auto *resultOrderPtr = builder.CreateCall(getOrderNewDecl(), {}, "transform.result");
    
    // Create iterator
    auto *iter = builder.CreateCall(getIterOrderDecl(), {collPtrVoid}, "transform.iter");
    
    // Create iteration variable slot
    auto *elemSlot = getOrCreateVar(transform->itemVar);
    
    auto *loopCond = BasicBlock::Create(ctx, "transform.cond", currentFunction);
    auto *loopBody = BasicBlock::Create(ctx, "transform.body", currentFunction);
    auto *loopEnd = BasicBlock::Create(ctx, "transform.end", currentFunction);
    
    builder.CreateBr(loopCond);
    
    builder.SetInsertPoint(loopCond);
    Value *hasNext = builder.CreateCall(getIterHasNextDecl(), {iter}, "hasNext");
    builder.CreateCondBr(hasNext, loopBody, loopEnd);
    
    builder.SetInsertPoint(loopBody);
    builder.CreateCall(getIterNextDecl(), {iter, elemSlot});
    
    // Compile transformation expression
    Value *transResult = compileExpr(transform->transformation.get());
    auto *transSlot = createAllocaInEntry("transform.elem");
    builder.CreateStore(transResult, transSlot);
    builder.CreateCall(getOrderPushDecl(), {resultOrderPtr, transSlot});
    builder.CreateBr(loopCond);
    
    builder.SetInsertPoint(loopEnd);
    builder.CreateCall(getIterFreeDecl(), {iter});
    
    // Wrap result order into ArdentValue
    auto *resultSlot = createAllocaInEntry("transform.out");
    builder.CreateCall(getMakeOrderDecl(), {resultOrderPtr, resultSlot});
    return builder.CreateLoad(getArdentValueTy(), resultSlot, "transform.val");
}

#endif // ARDENT_ENABLE_LLVM
