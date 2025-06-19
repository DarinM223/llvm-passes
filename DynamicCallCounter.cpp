#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"

using namespace llvm;

namespace {
struct DynamicCallCounter : PassInfoMixin<DynamicCallCounter> {
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) {
    LLVMContext &CTX = M.getContext();

    auto counterType = IntegerType::getInt32Ty(CTX);
    IRBuilder<> builder(CTX);
    bool changed = false;
    DenseMap<StringRef, Constant *> functionToGlobalMap;
    for (auto &F : M) {
      if (F.isDeclaration()) {
        continue;
      }

      changed = true;
      std::string functionGlobalName = std::string(F.getName()) + "_Counter";
      Constant *functionGlobal =
          M.getOrInsertGlobal(functionGlobalName, counterType);
      dyn_cast<GlobalVariable>(functionGlobal)
          ->setInitializer(builder.getInt32(0));
      functionToGlobalMap[F.getName()] = functionGlobal;

      // Increment counter inside function
      builder.SetInsertPoint(F.getEntryBlock().getFirstInsertionPt());
      auto load = builder.CreateLoad(counterType, functionGlobal);
      auto add = builder.CreateAdd(load, builder.getInt32(1));
      builder.CreateStore(add, functionGlobal);
    }
    if (!changed) {
      return PreservedAnalyses::all();
    }

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

    auto formatStr = ConstantDataArray::get(CTX, "%s -> %d\n");
    // Create global initialized with format string constant
    auto formatStrVar =
        M.getOrInsertGlobal("PrintfFormatStr", formatStr->getType());
    dyn_cast<GlobalVariable>(formatStrVar)->setInitializer(formatStr);

    // Create print function that prints all the created global variables
    auto printWrapperTy = FunctionType::get(Type::getVoidTy(CTX), {}, false);
    auto printWrapper = dyn_cast<Function>(
        M.getOrInsertFunction("print_wrapper", printWrapperTy).getCallee());
    auto block = BasicBlock::Create(CTX, "enter", printWrapper);
    builder.SetInsertPoint(block);
    for (auto [fnName, global] : functionToGlobalMap) {
      auto load = builder.CreateLoad(counterType, global);
      auto formatStrVarCast =
          builder.CreatePointerCast(formatStrVar, PrintfArgTy, "formatStr");
      auto funcName = builder.CreateGlobalStringPtr(fnName);
      builder.CreateCall(Printf, {formatStrVarCast, funcName, load});
    }
    builder.CreateRetVoid();

    // Calls print function when module is finished
    appendToGlobalDtors(M, printWrapper, 0);
    return PreservedAnalyses::none();
  }
  static bool isRequired() { return true; }
};
} // namespace

llvm::PassPluginLibraryInfo getDynamicCounterPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "DynamicCounter", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "dynamic-call-counter") {
                    MPM.addPass(DynamicCallCounter());
                    return true;
                  }
                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getDynamicCounterPluginInfo();
}