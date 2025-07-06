#include "RIV.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/RandomNumberGenerator.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include <map>
#include <random>

using namespace llvm;

namespace {
struct DuplicateBB : PassInfoMixin<DuplicateBB> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    if (!pRNG) {
      pRNG = F.getParent()->createRNG("duplicate-bb");
    }

    RIV::Result rivAnalysis = FAM.getResult<RIV>(F);
    std::map<BasicBlock *, Value *> targets;
    for (auto &BB : F) {
      auto &rivs = rivAnalysis[&BB];
      if (BB.isLandingPad() || rivs.empty()) {
        continue;
      }

      std::uniform_int_distribution<> Dist(0, rivs.size() - 1);
      auto it = rivs.begin();
      std::advance(it, Dist(*pRNG));
      targets.emplace(&BB, *it);
    }

    std::unordered_map<Value *, Value *> valueToPhi;
    for (auto [BB, checkValue] : targets) {
      Instruction *splitAt = BB->getFirstNonPHI();
      IRBuilder<> builder(splitAt);
      Value *cond = builder.CreateIsNull(valueToPhi.contains(checkValue)
                                             ? valueToPhi[checkValue]
                                             : checkValue);
      Instruction *thenTerm = nullptr, *elseTerm = nullptr;
      SplitBlockAndInsertIfThenElse(cond, splitAt, &thenTerm, &elseTerm);
      /*
       * At this point the blocks look like this:
       *
       *          +-------------------------+
       *          | if-then-else (new)      |
       *          | has instrs before split |
       *          +-------------------------+
       *          /                      \
       *    +----------+            +------------+
       *    | if (new) |            | else (new) |
       *    +----------+            +------------+
       *          \                      /
       *           \                    /
       *          +------------------------+
       *          | tail (original block)  |
       *          | has instrs after split |
       *          +------------------------+
       *
       * thenTerm is the terminator to the if block, elseTerm is the
       * terminator to the else block. To get the tail its
       * thenTerm->successor(0), to get the if-then-else block its the
       * thenTerm->getParent()->getSinglePredecessor().
       */
      auto TailBlock = thenTerm->getSuccessor(0);

      std::vector<Instruction *> freeList;
      ValueToValueMapTy tailVMap, thenVMap, elseVMap;
      for (auto it = TailBlock->begin(); it != TailBlock->end(); ++it) {
        Instruction &instr = *it;
        // Don't copy terminator.
        if (instr.isTerminator()) {
          RemapInstruction(&instr, tailVMap, RF_IgnoreMissingLocals);
          continue;
        }

        Instruction *thenClone = instr.clone(), *elseClone = instr.clone();

        RemapInstruction(thenClone, thenVMap, RF_IgnoreMissingLocals);
        thenClone->insertBefore(thenTerm);
        thenVMap[&instr] = thenClone;

        RemapInstruction(elseClone, elseVMap, RF_IgnoreMissingLocals);
        elseClone->insertBefore(elseTerm);
        elseVMap[&instr] = elseClone;

        // If instruction doesn't produce value, you can delete the instruction
        // from tail block.
        if (instr.getType()->isVoidTy()) {
          freeList.push_back(&instr);
          continue;
        }

        // Otherwise, replace instruction with phi node.
        PHINode *phi = PHINode::Create(instr.getType(), 2);
        phi->addIncoming(thenClone, thenClone->getParent());
        phi->addIncoming(elseClone, elseClone->getParent());
        tailVMap[&instr] = phi;
        valueToPhi[&instr] = phi;
        ReplaceInstWithInst(TailBlock, it, phi);
      }
      for (auto instr : freeList) {
        instr->eraseFromParent();
      }
    }
    return targets.empty() ? PreservedAnalyses::all()
                           : PreservedAnalyses::none();
  }
  static bool isRequired() { return true; }

private:
  std::unique_ptr<RandomNumberGenerator> pRNG;
};
} // namespace

llvm::PassPluginLibraryInfo getDuplicateBBPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "DuplicateBB", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "duplicate-bb") {
                    FPM.addPass(DuplicateBB());
                    return true;
                  }
                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getDuplicateBBPluginInfo();
}