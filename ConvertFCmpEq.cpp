#include "FindFCmpEq.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Module.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

namespace {
CmpInst::Predicate convertPredicate(FCmpInst *fcmp) noexcept {
  switch (fcmp->getPredicate()) {
  case CmpInst::Predicate::FCMP_OEQ:
    return CmpInst::Predicate::FCMP_OLT;
  case CmpInst::Predicate::FCMP_UEQ:
    return CmpInst::Predicate::FCMP_ULT;
  case CmpInst::Predicate::FCMP_ONE:
    return CmpInst::Predicate::FCMP_OGE;
  case CmpInst::Predicate::FCMP_UNE:
    return CmpInst::Predicate::FCMP_UGE;
  default:
    llvm_unreachable("unsupported fcmp predicate");
  }
}
bool convertFCmpEq(FCmpInst *fcmp) noexcept {
  if (!fcmp->isEquality()) {
    return false;
  }

  LLVMContext &CTX = fcmp->getModule()->getContext();
  auto I64Ty = IntegerType::get(CTX, 64);
  auto DoubleTy = Type::getDoubleTy(CTX);
  auto SignMask = ConstantInt::get(I64Ty, ~(1L << 63));
  APInt EpsilonBits(64, 0x3CB0000000000000);
  auto EpsilonValue = ConstantFP::get(DoubleTy, EpsilonBits.bitsToDouble());

  IRBuilder<> builder(fcmp);
  auto FSub = builder.CreateFSub(fcmp->getOperand(0), fcmp->getOperand(1));
  auto CastToI64 = builder.CreateBitCast(FSub, I64Ty);
  auto AbsValue = builder.CreateAnd(CastToI64, SignMask);
  auto CastToDouble = builder.CreateBitCast(AbsValue, DoubleTy);

  fcmp->setPredicate(convertPredicate(fcmp));
  fcmp->setOperand(0, CastToDouble);
  fcmp->setOperand(1, EpsilonValue);
  return true;
}

struct ConvertFCmpEq : PassInfoMixin<ConvertFCmpEq> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    FindFCmpEq::Result analysis = FAM.getResult<FindFCmpEq>(F);
    bool changed = false;
    if (!F.hasFnAttribute(Attribute::OptimizeNone)) {
      for (auto fcmp : analysis) {
        changed |= convertFCmpEq(fcmp);
      }
    }
    return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
  }
  static bool isRequired() { return true; }
};
} // namespace

llvm::PassPluginLibraryInfo getConvertFCmpEqPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "FCmpEq", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "convert-fcmp-eq") {
                    FPM.addPass(ConvertFCmpEq());
                    return true;
                  }
                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getConvertFCmpEqPluginInfo();
}