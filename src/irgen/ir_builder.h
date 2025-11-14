#pragma once
#include <string>
#include <vector>
#include <memory>

// Forward declare LLVM types to avoid hard dependency when ARDENT_ENABLE_LLVM=OFF
#ifdef ARDENT_ENABLE_LLVM
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#endif

namespace ardent::irgen {

struct IRBuildResult {
    bool ok{false};
    std::string error;
};

#ifdef ARDENT_ENABLE_LLVM
// Build a demo module returning ardent_rt_add(2,3)
std::unique_ptr<llvm::Module> build_demo_add_module(llvm::LLVMContext &ctx);
#endif

} // namespace ardent::irgen
