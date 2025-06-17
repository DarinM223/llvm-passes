#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

struct OpcodeCounter : AnalysisInfoMixin<OpcodeCounter> {
  using Result = StringMap<int>;

  OpcodeCounter::Result run(Function &F, FunctionAnalysisManager &) {
    OpcodeCounter::Result results;
    for (auto &BB : F) {
      for (auto &I : BB) {
        StringRef opcode = I.getOpcodeName();
        if (results.contains(opcode)) {
          results[opcode]++;
        } else {
          results[opcode] = 1;
        }
      }
    }
    return results;
  }

  static bool isRequired() { return true; }

private:
  static inline AnalysisKey Key = {};
  friend struct AnalysisInfoMixin<OpcodeCounter>;
};

struct OpcodeCounterPrinter : PassInfoMixin<OpcodeCounterPrinter> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    OpcodeCounter::Result analysis = FAM.getResult<OpcodeCounter>(F);
    errs() << "Analysis: of " << F.getName() << "\n";
    for (auto key : analysis.keys()) {
      errs() << key << " -> " << analysis[key] << "\n";
    }
    return PreservedAnalyses::all();
  }
  static bool isRequired() { return true; }
};

} // namespace

llvm::PassPluginLibraryInfo getOpCodeCounterPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "OpcodeCounter", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "opcode-counter") {
                    FPM.addPass(OpcodeCounterPrinter());
                    return true;
                  }
                  return false;
                });

            PB.registerVectorizerStartEPCallback(
                [](FunctionPassManager &FPM, OptimizationLevel) {
                  FPM.addPass(OpcodeCounterPrinter());
                });

            PB.registerAnalysisRegistrationCallback(
                [](FunctionAnalysisManager &FAM) {
                  FAM.registerPass([&] { return OpcodeCounter(); });
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getOpCodeCounterPluginInfo();
}