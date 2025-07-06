#include "llvm/ADT/Statistic.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

#define DEBUG_TYPE "mba-sub"

STATISTIC(SubstCount, "The # of substituted instructions");

namespace {
struct MBASub : PassInfoMixin<MBASub> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    bool changed = false;
    std::vector<BinaryOperator *> freeList;
    for (auto &BB : F) {
      for (auto &I : BB) {
        BinaryOperator *binop;
        if ((binop = dyn_cast<BinaryOperator>(&I)) &&
            binop->getOpcode() == Instruction::Sub &&
            binop->getType()->isIntegerTy()) {
          IRBuilder<> builder(binop);
          auto Not = builder.CreateNot(binop->getOperand(1));
          auto Add = builder.CreateAdd(binop->getOperand(0), Not);
          auto One = ConstantInt::get(binop->getType(), 1);
          auto Incr = builder.CreateAdd(Add, One);
          LLVM_DEBUG(dbgs() << *binop << " -> " << *Incr << "\n");
          binop->replaceAllUsesWith(Incr);
          freeList.push_back(binop);
          changed = true;
          SubstCount++;
        }
      }
    }
    for (auto binop : freeList) {
      binop->eraseFromParent();
    }
    return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
  }
  static bool isRequired() { return true; }
};
} // namespace

llvm::PassPluginLibraryInfo getMBASubPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "MBASub", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "mba-sub") {
                    FPM.addPass(MBASub());
                    return true;
                  }
                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getMBASubPluginInfo();
}