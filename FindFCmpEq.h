#ifndef FIND_FCMP_EQ_H
#define FIND_FCMP_EQ_H

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/PassManager.h"

namespace llvm {
class FCmpInst;
}

struct FindFCmpEq : llvm::AnalysisInfoMixin<FindFCmpEq> {
  using Result = std::vector<llvm::FCmpInst *>;
  Result run(llvm::Function &F, llvm::FunctionAnalysisManager &FAM);

private:
  static inline llvm::AnalysisKey Key;
  friend struct llvm::AnalysisInfoMixin<FindFCmpEq>;
};

#endif