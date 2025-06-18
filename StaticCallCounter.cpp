#include "llvm/IR/Module.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

namespace {
struct StaticCallCounter : AnalysisInfoMixin<StaticCallCounter> {
  using Result = MapVector<const Function *, unsigned>;

  Result run(Module &M, ModuleAnalysisManager &) {
    Result result;
    for (auto &F : M) {
      for (auto &BB : F) {
        for (auto &I : BB) {
          CallBase *CB;
          Function *DirectInvoc;
          // CallBase is the base subclass of Instruction,
          // CallInst and InvokeInst inherit off of it.
          if ((CB = dyn_cast<CallBase>(&I)) &&
              (DirectInvoc = CB->getCalledFunction())) {
            if (result.contains(DirectInvoc)) {
              result[DirectInvoc]++;
            } else {
              result[DirectInvoc] = 1;
            }
          }
        }
      }
    }
    return result;
  }
  static bool isRequired() { return true; }

private:
  static inline AnalysisKey Key;
  friend struct AnalysisInfoMixin<StaticCallCounter>;
};

struct StaticCallCounterPrinter : PassInfoMixin<StaticCallCounterPrinter> {
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) {
    StaticCallCounter::Result analysis = MAM.getResult<StaticCallCounter>(M);
    for (auto [K, V] : analysis) {
      errs() << K->getName() << " -> " << V << "\n";
    }
    return PreservedAnalyses::all();
  }
  static bool isRequired() { return true; }
};
} // namespace

llvm::PassPluginLibraryInfo getStaticCallCounterPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "StaticCallCounter", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "static-call-counter") {
                    MPM.addPass(StaticCallCounterPrinter());
                    return true;
                  }
                  return false;
                });
            PB.registerAnalysisRegistrationCallback(
                [](ModuleAnalysisManager &MAM) {
                  MAM.registerPass([&] { return StaticCallCounter(); });
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getStaticCallCounterPluginInfo();
}