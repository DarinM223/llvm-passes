#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

namespace {
struct MBAAdd : PassInfoMixin<MBAAdd> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    LLVMContext &CTX = F.getContext();
    std::vector<BinaryOperator *> freeList;
    for (auto &BB : F) {
      for (auto &I : BB) {
        BinaryOperator *binop;
        if ((binop = dyn_cast<BinaryOperator>(&I)) &&
            binop->getOpcode() == Instruction::Add &&
            binop->getType() == IntegerType::getInt8Ty(CTX)) {
          IRBuilder<> builder(binop);
          auto Xor =
              builder.CreateXor(binop->getOperand(0), binop->getOperand(1));
          auto Bitand =
              builder.CreateAnd(binop->getOperand(0), binop->getOperand(1));
          auto Combined = builder.CreateAdd(
              Xor,
              builder.CreateMul(ConstantInt::get(binop->getType(), 2), Bitand));
          auto WithNums1 = builder.CreateAdd(
              builder.CreateMul(Combined,
                                ConstantInt::get(binop->getType(), 39)),
              ConstantInt::get(binop->getType(), 23));
          auto WithNums2 = builder.CreateAdd(
              builder.CreateMul(WithNums1,
                                ConstantInt::get(binop->getType(), 151)),
              ConstantInt::get(binop->getType(), 111));
          binop->replaceAllUsesWith(WithNums2);
          freeList.push_back(binop);
        }
      }
    }
    for (auto binop : freeList) {
      binop->eraseFromParent();
    }
    return PreservedAnalyses::all();
  }
  static bool isRequired() { return true; }
};
} // namespace

llvm::PassPluginLibraryInfo getMBAAddPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "MBAAdd", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "mba-add") {
                    FPM.addPass(MBAAdd());
                    return true;
                  }
                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getMBAAddPluginInfo();
}