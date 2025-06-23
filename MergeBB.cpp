#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include <ranges>

using namespace llvm;

namespace {
bool canRemoveInst(const Instruction *inst) {
  const Instruction *user = dyn_cast<Instruction>(*inst->user_begin());
  BasicBlock *succ = inst->getParent()->getTerminator()->getSuccessor(0);

  const PHINode *phiUser;
  bool usedInPhi =
      ((phiUser = dyn_cast<PHINode>(user)) && phiUser->getParent() == succ &&
       phiUser->getIncomingValueForBlock(inst->getParent()) == inst);

  return user->getParent() == inst->getParent() || usedInPhi;
}

bool identicalInstructions(const Instruction *inst1, const Instruction *inst2) {
  if (!inst1->isSameOperationAs(inst2)) {
    return false;
  }
  if (!((inst1->user_empty() || inst1->hasOneUse()) &&
        inst1->getNumUses() == inst2->getNumUses())) {
    return false;
  }
  if (inst1->hasOneUse() && (!canRemoveInst(inst1) || !canRemoveInst(inst2))) {
    return false;
  }
  if (inst1->getNumOperands() != inst2->getNumOperands()) {
    return false;
  }
  for (unsigned i = 0; i < inst1->getNumOperands(); ++i) {
    if (inst2->getOperand(i) != inst1->getOperand(i)) {
      return false;
    }
  }
  return true;
}

unsigned getNumNonDbgInstrs(BasicBlock *BB) {
  return std::ranges::count_if(
      *BB, [](Instruction &I) { return !isa<DbgInfoIntrinsic>(I); });
}

class LockstepReverseIterator {
  BasicBlock *bb1;
  BasicBlock *bb2;
  bool fail;
  SmallVector<Instruction *, 2> insts;

public:
  Instruction *getLastNonDbgInst(BasicBlock *bb) {
    auto inst = bb->getTerminator();
    do {
      inst = inst->getPrevNode();
    } while (inst && isa<DbgInfoIntrinsic>(inst));
    return inst;
  }
  LockstepReverseIterator(BasicBlock *bb1, BasicBlock *bb2)
      : bb1(bb1), bb2(bb2) {
    auto inst1 = getLastNonDbgInst(bb1);
    auto inst2 = getLastNonDbgInst(bb2);
    fail = !inst1 || !inst2;
    insts = {inst1, inst2};
  }
  bool isValid() const { return !fail; }
  void operator--() {
    if (fail) {
      return;
    }

    for (auto &inst : insts) {
      do {
        inst = inst->getPrevNode();
      } while (inst && isa<DbgInfoIntrinsic>(inst));
      if (!inst) {
        fail = true;
        return;
      }
    }
  }
  SmallVector<Instruction *, 2> operator*() const { return insts; }
};

struct MergeBB : PassInfoMixin<MergeBB> {
  /**
   * Rewrite block predecessors to jump to the retained block instead of the
   * erased block.
   */
  void updateBranchTargets(BasicBlock *bbToErase, BasicBlock *bbToRetain) {
    for (auto pred : predecessors(bbToErase)) {
      auto term = pred->getTerminator();
      for (unsigned i = 0; i < term->getNumOperands(); ++i) {
        if (term->getOperand(i) == bbToErase) {
          term->setOperand(i, bbToRetain);
        }
      }
    }
  }
  bool mergeDuplicatedBlock(BasicBlock *BB,
                            SmallPtrSet<BasicBlock *, 8> &DeleteList) {
    if (BB == &BB->getParent()->getEntryBlock()) {
      return false;
    }

    auto branchTerm = dyn_cast<BranchInst>(BB->getTerminator());
    if (!(branchTerm && branchTerm->isUnconditional())) {
      return false;
    }

    for (auto pred : predecessors(BB)) {
      if (!(isa<BranchInst>(pred->getTerminator()) ||
            isa<SwitchInst>(pred->getTerminator()))) {
        return false;
      }
    }

    BasicBlock *succ = branchTerm->getSuccessor(0);
    auto it = succ->begin();
    auto phiNode = dyn_cast<PHINode>(it);
    Value *inVal1 = nullptr, *inVal2 = nullptr;
    Instruction *inInst1 = nullptr, *inInst2 = nullptr;
    if (phiNode) {
      // Do not optimize if successor has multiple phi nodes.
      if (++it != succ->end() && isa<PHINode>(it)) {
        return false;
      }
      inVal1 = phiNode->getIncomingValueForBlock(BB);
      inInst1 = dyn_cast<Instruction>(inVal1);
    }

    unsigned numInst = getNumNonDbgInstrs(BB);
    for (auto BB2 : predecessors(succ)) {
      if (BB2 == &BB2->getParent()->getEntryBlock()) {
        continue;
      }
      auto branchTerm = dyn_cast<BranchInst>(BB2->getTerminator());
      if (!(branchTerm && branchTerm->isUnconditional())) {
        continue;
      }
      for (auto pred : predecessors(BB2)) {
        if (!(isa<BranchInst>(pred->getTerminator()) ||
              isa<SwitchInst>(pred->getTerminator()))) {
          // NOTE: can't do continue in here because its a nested for loop
          goto continue2;
        }
      }
      if (DeleteList.contains(BB2) || BB == BB2 ||
          numInst != getNumNonDbgInstrs(BB2)) {
        continue;
      }

      if (phiNode) {
        inVal2 = phiNode->getIncomingValueForBlock(BB2);
        inInst2 = dyn_cast<Instruction>(inVal2);
        bool bothValuesDefinedInParent =
            ((inInst1 && inInst1->getParent() == BB) &&
             (inInst2 && inInst2->getParent() == BB2));
        if (inVal1 != inVal2 && !bothValuesDefinedInParent) {
          continue;
        }
      }

      {
        LockstepReverseIterator it(BB, BB2);
        while (it.isValid() && identicalInstructions((*it)[0], (*it)[1])) {
          --it;
        }
        if (it.isValid()) {
          continue;
        }

        updateBranchTargets(BB, BB2);
        DeleteList.insert(BB);
        return true;
      }

    continue2:
      (void)0;
    }
    return false;
  }
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    SmallPtrSet<BasicBlock *, 8> DeleteList;
    bool changed = false;
    for (auto &BB : F) {
      changed |= mergeDuplicatedBlock(&BB, DeleteList);
    }
    for (auto BB : DeleteList) {
      DeleteDeadBlock(BB);
    }
    return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
  }
  static bool isRequired() { return true; }
};
} // namespace

llvm::PassPluginLibraryInfo getMergeBBPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "MergeBB", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "merge-bb") {
                    FPM.addPass(MergeBB());
                    return true;
                  }
                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getMergeBBPluginInfo();
}