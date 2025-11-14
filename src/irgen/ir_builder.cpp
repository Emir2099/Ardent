#include "ir_builder.h"
#include "../runtime/ardent_runtime.h"
#ifdef ARDENT_ENABLE_LLVM
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Verifier.h>
#endif

namespace ardent::irgen {

#ifdef ARDENT_ENABLE_LLVM
std::unique_ptr<llvm::Module> build_demo_add_module(llvm::LLVMContext &ctx) {
    using namespace llvm;
    auto mod = std::make_unique<Module>("ardent_demo", ctx);

    // Define ArdentValue struct: { i8 tag, [7 x i8] pad, i64 payload }
    auto int8Ty  = Type::getInt8Ty(ctx);
    auto int64Ty = Type::getInt64Ty(ctx);
    auto padTy   = ArrayType::get(int8Ty, 7);
    auto valTy   = StructType::create(ctx, "ArdentValue");
    valTy->setBody({ int8Ty, padTy, int64Ty });

    // Runtime function prototype: i64 ardent_rt_add_i64(i64, i64)
    auto fnAddTy = FunctionType::get(int64Ty, { int64Ty, int64Ty }, false);
    Function::Create(fnAddTy, Function::ExternalLinkage, "ardent_rt_add_i64", mod.get());

    // ardent_demo(): ArdentValue
    auto mainTy = FunctionType::get(valTy, {}, false);
    auto mainFn = Function::Create(mainTy, Function::ExternalLinkage, "ardent_demo", mod.get());
    auto entry  = BasicBlock::Create(ctx, "entry", mainFn);
    IRBuilder<> b(entry);

    // Construct ArdentValue constants for 2 and 3
    auto makeNumber = [&](int64_t v) -> Value* {
        Value *undef = UndefValue::get(valTy);
        // tag = ARD_NUMBER (0)
        undef = b.CreateInsertValue(undef, b.getInt8(0), {0});
        // payload = v (field index 2)
        undef = b.CreateInsertValue(undef, ConstantInt::get(int64Ty, v), {2});
        return undef;
    };
    // Call ardent_rt_add_i64(2,3) and wrap into ArdentValue
    auto rtAdd = mod->getFunction("ardent_rt_add_i64");
    Value *sum64 = b.CreateCall(rtAdd, { ConstantInt::get(int64Ty, 2), ConstantInt::get(int64Ty, 3) }, "sum64");
    Value *retv = UndefValue::get(valTy);
    retv = b.CreateInsertValue(retv, b.getInt8(0), {0});
    retv = b.CreateInsertValue(retv, sum64, {2});
    b.CreateRet(retv);

    verifyModule(*mod, &llvm::errs());
    return mod;
}
#endif

} // namespace ardent::irgen
