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
#ifdef ARDENT_ENABLE_LLVM
    std::shared_ptr<llvm::Module> module;
#endif
};

class IRBuilderFacade {
public:
    IRBuilderFacade();
#ifdef ARDENT_ENABLE_LLVM
    llvm::LLVMContext &context();
#endif
    // Build IR from a high-level placeholder (later AST root)
    IRBuildResult buildDemoAddFunction();
};

} // namespace ardent::irgen
