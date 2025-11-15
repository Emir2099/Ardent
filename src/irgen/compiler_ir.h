#pragma once
#ifdef ARDENT_ENABLE_LLVM
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include "../ast.h"
#include <string>
#include <unordered_map>
#include <llvm/IR/IRBuilder.h>

// Forward runtime wrapper (placeholder until fleshed out)
struct ArdentRuntime { };

class IRCompiler {
public:
    IRCompiler(llvm::LLVMContext &ctx, llvm::Module &mod, ArdentRuntime &rt);

    llvm::Value* compileExpr(ASTNode*); // Placeholder: will refine to Expression hierarchy later
    void compileStmt(ASTNode*);
    void compileSpell(SpellStatement*);
    void compileProgram(BlockStatement*); // Temporary: program root abstraction pending

    // PHASE 2: ArdentValue ABI in IR
    llvm::StructType* getArdentValueTy();
    llvm::Value* makeNumber(int64_t v);
    llvm::Value* makeBoolean(bool b);
    llvm::Value* makePhrase(const std::string &s);

    // PHASE 3 helpers
    void setInsertionPoint(llvm::BasicBlock *bb) { builder.SetInsertPoint(bb); }
    void setFunction(llvm::Function *fn) { currentFunction = fn; }
    void bindAlloca(const std::string &name, llvm::AllocaInst *slot) { namedValues[name] = slot; }
    llvm::AllocaInst* createAllocaInEntry(const std::string &name);
    llvm::AllocaInst* getOrCreateVar(const std::string &name);
    llvm::Value* extractBoolean(llvm::Value* ardentV);
    llvm::Function* getPrintDecl();
    llvm::Function* getOrCreateSpellDecl(const std::string &name, size_t paramCount);
    llvm::Function* getConcatDecl(); // void concat(ptr a, ptr b, ptr out)
private:
    llvm::LLVMContext &ctx;
    llvm::Module &mod;
    ArdentRuntime &runtime;

    // Cached ABI struct
    llvm::StructType *ardentValueTy = nullptr;
    unsigned nextStringId = 0;

    // Codegen state
    llvm::IRBuilder<> builder{ctx};
    llvm::Function *currentFunction = nullptr;
    std::unordered_map<std::string, llvm::AllocaInst*> namedValues;

    // Internal helpers
    llvm::Function* getIntBinOpDecl(const char* name);
    llvm::Value* extractNumber(llvm::Value* ardentV);
    llvm::Value* buildNumberFromI64(llvm::Value* v);
    llvm::Value* buildBooleanFromI1(llvm::Value* v);
    llvm::Function* getConcatDeclInternal();
};
#endif // ARDENT_ENABLE_LLVM
