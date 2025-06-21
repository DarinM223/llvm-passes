#include "RIV.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Module.h"
#include <deque>

using namespace llvm;

RIV::Result RIV::run(Function &F, FunctionAnalysisManager &FAM) {
  DominatorTree *domTree = &FAM.getResult<DominatorTreeAnalysis>(F);
  Result result;

  Result definedValues;
  for (auto &BB : F) {
    auto &Values = definedValues[&BB];
    for (auto &I : BB) {
      if (I.getType()->isIntegerTy()) {
        Values.insert(&I);
      }
    }
  }

  auto &entryValues = result[&F.getEntryBlock()];
  for (auto &global : F.getParent()->global_values()) {
    if (global.getType()->isIntegerTy()) {
      entryValues.insert(&global);
    }
  }
  for (auto &arg : F.args()) {
    if (arg.getType()->isIntegerTy()) {
      entryValues.insert(&arg);
    }
  }

  std::deque<DomTreeNode *> worklist;
  worklist.push_back(domTree->getRootNode());

  while (!worklist.empty()) {
    auto node = worklist.back();
    worklist.pop_back();

    auto &defs = definedValues[node->getBlock()];
    // Make a copy of the values, not a reference.
    auto rivs = result[node->getBlock()];

    for (auto child : node->children()) {
      worklist.push_back(child);
      result[child->getBlock()].insert(defs.begin(), defs.end());
      result[child->getBlock()].insert(rivs.begin(), rivs.end());
    }
  }
  return result;
}

struct RIVPrinter : PassInfoMixin<RIVPrinter> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    RIV::Result analysis = FAM.getResult<RIV>(F);
    for (auto [BB, ValSet] : analysis) {
      BB->printAsOperand(errs());
      errs() << " -> {\n";
      for (auto V : ValSet) {
        V->print(errs());
        errs() << "\n";
      }
      errs() << "}\n";
    }
    return PreservedAnalyses::all();
  }
};

llvm::PassPluginLibraryInfo getRIVPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "RIV", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "riv") {
                    FPM.addPass(RIVPrinter());
                    return true;
                  }
                  return false;
                });
            PB.registerAnalysisRegistrationCallback(
                [](FunctionAnalysisManager &FAM) {
                  FAM.registerPass([&] { return RIV(); });
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getRIVPluginInfo();
}