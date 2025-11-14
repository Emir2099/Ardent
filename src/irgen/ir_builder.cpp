#include "ir_builder.h"
#include <iostream>

namespace ardent::irgen {

IRBuilderFacade::IRBuilderFacade() {
#ifdef ARDENT_ENABLE_LLVM
    // Nothing yet; context created implicitly when module made.
#endif
}

#ifdef ARDENT_ENABLE_LLVM
llvm::LLVMContext &IRBuilderFacade::context() {
    static llvm::LLVMContext ctx; // singleton for now
    return ctx;
}
#endif

IRBuildResult IRBuilderFacade::buildDemoAddFunction() {
    IRBuildResult R;
#ifdef ARDENT_ENABLE_LLVM
    auto &ctx = context();
    auto mod = std::make_shared<llvm::Module>("ardent_demo", ctx);
    llvm::IRBuilder<> b(ctx);
    // int add(int a, int b)
    auto *i32 = llvm::Type::getInt32Ty(ctx);
    auto *fnTy = llvm::FunctionType::get(i32, {i32, i32}, false);
    auto *fn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "add", mod.get());
    auto entry = llvm::BasicBlock::Create(ctx, "entry", fn);
    b.SetInsertPoint(entry);
    auto argsIt = fn->arg_begin();
    llvm::Value *a = argsIt++;
    llvm::Value *c = argsIt++;
    auto *sum = b.CreateAdd(a, c, "sum");
    b.CreateRet(sum);
    R.ok = true;
    R.module = mod;
#else
    R.ok = false;
    R.error = "LLVM disabled (ARDENT_ENABLE_LLVM=OFF)";
#endif
    return R;
}

} // namespace ardent::irgen
