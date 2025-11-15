#ifdef ARDENT_ENABLE_LLVM
#include <iostream>
#include <memory>
#include <llvm/Support/TargetSelect.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>
#include <llvm/ExecutionEngine/Orc/Mangling.h>
#include <llvm/IR/LLVMContext.h>
#include "ir_builder.h"
#include "../runtime/ardent_runtime.h"

using namespace llvm;
using namespace llvm::orc;

extern "C" void __main();

int main() {
    // Initialize native targets
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetAsmParser();

    auto tspCtx = std::make_unique<LLVMContext>();
    LLVMContext &ctx = *tspCtx;
    auto mod = ardent::irgen::build_demo_add_module(ctx);

    auto jitExpected = LLJITBuilder().create();
    if (!jitExpected) {
        std::cerr << "Failed to create LLJIT: " << toString(jitExpected.takeError()) << std::endl;
        return 1;
    }
    auto jit = std::move(*jitExpected);

    // Make external symbols available to JIT:
    // 1) Search current process (CRT like __main, etc.)
    {
        auto gen = cantFail(DynamicLibrarySearchGenerator::GetForCurrentProcess(
            jit->getDataLayout().getGlobalPrefix()));
        jit->getMainJITDylib().addGenerator(std::move(gen));
    }

    // 2) Manually map Ardent runtime helpers that are linked statically
    {
        MangleAndInterner M(jit->getExecutionSession(), jit->getDataLayout());
        SymbolMap symbols;
        symbols[M("ardent_rt_add_i64")] = ExecutorSymbolDef(ExecutorAddr::fromPtr(&ardent_rt_add_i64), JITSymbolFlags::Exported);
        symbols[M("ardent_rt_sub_i64")] = ExecutorSymbolDef(ExecutorAddr::fromPtr(&ardent_rt_sub_i64), JITSymbolFlags::Exported);
        symbols[M("ardent_rt_mul_i64")] = ExecutorSymbolDef(ExecutorAddr::fromPtr(&ardent_rt_mul_i64), JITSymbolFlags::Exported);
        symbols[M("ardent_rt_div_i64")] = ExecutorSymbolDef(ExecutorAddr::fromPtr(&ardent_rt_div_i64), JITSymbolFlags::Exported);
        symbols[M("ardent_rt_print")]   = ExecutorSymbolDef(ExecutorAddr::fromPtr(&ardent_rt_print), JITSymbolFlags::Exported);
        symbols[M("ardent_rt_version")] = ExecutorSymbolDef(ExecutorAddr::fromPtr(&ardent_rt_version), JITSymbolFlags::Exported);
        symbols[M("__main")]            = ExecutorSymbolDef(ExecutorAddr::fromPtr(&__main), JITSymbolFlags::Exported);
        if (auto err = jit->getMainJITDylib().define(absoluteSymbols(symbols))) {
            std::cerr << "Failed to define absolute symbols: " << toString(std::move(err)) << std::endl;
            return 1;
        }
    }

    ThreadSafeModule tsm(std::move(mod), std::move(tspCtx));
    if (auto err = jit->addIRModule(std::move(tsm))) {
        std::cerr << "Failed to add module: " << toString(std::move(err)) << std::endl;
        return 1;
    }

    std::cerr << "LLJIT created" << std::endl;
    auto symExpected = jit->lookup("ardent_demo");
    if (!symExpected) {
        std::cerr << "Symbol 'ardent_demo' not found: " << toString(symExpected.takeError()) << std::endl;
        return 1;
    }
    using Fn = ArdentValue(*)();
    Fn fn = symExpected->toPtr<Fn>();
    std::cerr << "Invoking JIT..." << std::endl;
    ArdentValue result = fn();
    std::cerr << "JIT returned" << std::endl;
    std::cout << "JIT result: " << result.num << std::endl;
    return 0;
}
#else
int main(){ return 0; }
#endif
