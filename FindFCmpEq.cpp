#include "FindFCmpEq.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/ModuleSlotTracker.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

FindFCmpEq::Result FindFCmpEq::run(Function &F, FunctionAnalysisManager &FAM) {
  Result result;
  for (auto &I : instructions(F)) {
    FCmpInst *fcmp;
    if (I.getOpcode() == Instruction::FCmp && (fcmp = dyn_cast<FCmpInst>(&I)) &&
        fcmp->isEquality()) {
      result.push_back(fcmp);
    }
  }
  return result;
}

struct FindFCmpEqPrinter : PassInfoMixin<FindFCmpEqPrinter> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    auto &analysis = FAM.getResult<FindFCmpEq>(F);
    if (analysis.empty()) {
      return PreservedAnalyses::all();
    }

    ModuleSlotTracker tracker(F.getParent());
    errs() << "Floating point equality comparisons in " << F.getName() << ":\n";
    for (auto fcmp : analysis) {
      fcmp->print(errs(), tracker);
      errs() << '\n';
    }
    return PreservedAnalyses::all();
  }
  static bool isRequired() { return true; }
};

llvm::PassPluginLibraryInfo getFindFCmpEqPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "FindFCmpEq", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "find-fcmp-eq") {
                    FPM.addPass(FindFCmpEqPrinter());
                    return true;
                  }
                  return false;
                });
            PB.registerAnalysisRegistrationCallback(
                [](FunctionAnalysisManager &FAM) {
                  FAM.registerPass([&] { return FindFCmpEq(); });
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getFindFCmpEqPluginInfo();
}