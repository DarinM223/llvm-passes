#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

namespace {
struct InjectFuncCall : PassInfoMixin<InjectFuncCall> {
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
    LLVMContext &CTX = M.getContext();
    PointerType *PrintfArgTy =
        PointerType::getUnqual(IntegerType::getInt8Ty(CTX));
    FunctionType *PrintfTy =
        FunctionType::get(IntegerType::getInt32Ty(CTX), PrintfArgTy, true);
    FunctionCallee Printf = M.getOrInsertFunction("printf", PrintfTy);

    // Set attributes for Printf
    {
      auto PrintfF = dyn_cast<Function>(Printf.getCallee());
      PrintfF->setDoesNotThrow();
      PrintfF->addParamAttr(0, Attribute::NoCapture);
      PrintfF->addParamAttr(0, Attribute::ReadOnly);
    }

    IRBuilder<> builder(CTX);

    auto formatStr = ConstantDataArray::get(
        CTX, "Hello from %s\n   Number of arguments: %d\n");

    // Create global initialized with format string constant
    auto formatStrVar =
        M.getOrInsertGlobal("PrintfFormatStr", formatStr->getType());
    dyn_cast<GlobalVariable>(formatStrVar)->setInitializer(formatStr);

    bool changed = false;
    for (auto &F : M) {
      if (F.isDeclaration()) {
        continue;
      }

      builder.SetInsertPoint(F.getEntryBlock().getFirstInsertionPt());
      // Create pointer cast to printf format string arg type
      auto formatStrVarCast =
          builder.CreatePointerCast(formatStrVar, PrintfArgTy, "formatStr");
      auto funcName = builder.CreateGlobalStringPtr(F.getName());
      auto args = builder.getInt32(F.arg_size());
      builder.CreateCall(Printf, {formatStrVarCast, funcName, args});
      changed = true;
    }
    return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
  }
};
} // namespace

llvm::PassPluginLibraryInfo getInjectFuncCallPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "InjectFuncCall", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "injectfunccall") {
                    MPM.addPass(InjectFuncCall());
                    return true;
                  }
                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getInjectFuncCallPluginInfo();
}