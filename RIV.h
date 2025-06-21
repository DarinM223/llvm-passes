#ifndef RIV_H
#define RIV_H

#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

struct RIV : llvm::AnalysisInfoMixin<RIV> {
  using Result = llvm::MapVector<llvm::BasicBlock const *,
                                 llvm::SmallPtrSet<llvm::Value *, 8>>;

  Result run(llvm::Function &F, llvm::FunctionAnalysisManager &FAM);

private:
  static inline llvm::AnalysisKey Key;
  friend struct AnalysisInfoMixin<RIV>;
};

#endif