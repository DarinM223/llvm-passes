#include "ast.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Scalar/Reassociate.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"
#include <ranges>

using namespace llvm;

std::unique_ptr<llvm::LLVMContext> TheContext;
std::unique_ptr<llvm::IRBuilder<>> TheBuilder;
std::unique_ptr<llvm::Module> TheModule;
llvm::StringMap<llvm::Value *> NamedValues;
llvm::StringMap<std::unique_ptr<PrototypeAST>> FunctionProtos;

std::unique_ptr<llvm::FunctionPassManager> TheFPM;
std::unique_ptr<llvm::LoopAnalysisManager> TheLAM;
std::unique_ptr<llvm::FunctionAnalysisManager> TheFAM;
std::unique_ptr<llvm::CGSCCAnalysisManager> TheCGAM;
std::unique_ptr<llvm::ModuleAnalysisManager> TheMAM;
std::unique_ptr<llvm::PassInstrumentationCallbacks> ThePIC;
std::unique_ptr<llvm::StandardInstrumentations> TheSI;

void initializeModuleAndManagers(const DataLayout &layout) {
  TheContext = std::make_unique<LLVMContext>();
  TheBuilder = std::make_unique<IRBuilder<>>(*TheContext);
  TheModule = std::make_unique<Module>("my cool jit", *TheContext);
  TheModule->setDataLayout(layout);

  TheFPM = std::make_unique<FunctionPassManager>();
  TheLAM = std::make_unique<LoopAnalysisManager>();
  TheFAM = std::make_unique<FunctionAnalysisManager>();
  TheCGAM = std::make_unique<CGSCCAnalysisManager>();
  TheMAM = std::make_unique<ModuleAnalysisManager>();
  ThePIC = std::make_unique<PassInstrumentationCallbacks>();
  TheSI = std::make_unique<StandardInstrumentations>(*TheContext, true);
  TheSI->registerCallbacks(*ThePIC, TheMAM.get());

  TheFPM->addPass(InstCombinePass());
  TheFPM->addPass(ReassociatePass());
  TheFPM->addPass(GVNPass());
  TheFPM->addPass(SimplifyCFGPass());

  PassBuilder PB;
  PB.registerModuleAnalyses(*TheMAM);
  PB.registerFunctionAnalyses(*TheFAM);
  PB.crossRegisterProxies(*TheLAM, *TheFAM, *TheCGAM, *TheMAM);
}

Function *getFunction(StringRef name) {
  if (auto *fn = TheModule->getFunction(name)) {
    return fn;
  }
  if (FunctionProtos.contains(name)) {
    return FunctionProtos[name]->codegen();
  }
  return nullptr;
}

Value *NumberExprAST::codegen() {
  return ConstantFP::get(*TheContext, APFloat(val_));
}

Value *VariableExprAST::codegen() {
  if (!NamedValues.contains(name_)) {
    throw CodegenException("Variable " + name_ +
                           " can't be found in environment");
  }
  return NamedValues.lookup(name_);
}

Value *BinaryExprAST::codegen() {
  auto lhs = lhs_->codegen();
  auto rhs = rhs_->codegen();
  switch (op_) {
  case '+':
    return TheBuilder->CreateFAdd(lhs, rhs);
  case '-':
    return TheBuilder->CreateFSub(lhs, rhs);
  case '*':
    return TheBuilder->CreateFMul(lhs, rhs);
  case '<': {
    auto cmp = TheBuilder->CreateFCmpULT(lhs, rhs);
    return TheBuilder->CreateUIToFP(cmp, Type::getDoubleTy(*TheContext));
  }
  default:
    throw CodegenException("Invalid binary op");
  }
}

Value *IfExprAST::codegen() {
  auto cond = cond_->codegen();
  cond = TheBuilder->CreateFCmpONE(cond,
                                   ConstantFP::get(*TheContext, APFloat(0.0)));

  auto fn = TheBuilder->GetInsertBlock()->getParent();

  auto thenBB = BasicBlock::Create(*TheContext, "then", fn);
  auto elseBB = BasicBlock::Create(*TheContext, "else");
  auto mergeBB = BasicBlock::Create(*TheContext, "merge");
  TheBuilder->CreateCondBr(cond, thenBB, elseBB);

  TheBuilder->SetInsertPoint(thenBB);
  auto thenValue = then_->codegen();
  TheBuilder->CreateBr(mergeBB);
  thenBB = TheBuilder->GetInsertBlock();

  fn->insert(fn->end(), elseBB);
  TheBuilder->SetInsertPoint(elseBB);
  auto elseValue = else_->codegen();
  TheBuilder->CreateBr(mergeBB);
  elseBB = TheBuilder->GetInsertBlock();

  fn->insert(fn->end(), mergeBB);
  TheBuilder->SetInsertPoint(mergeBB);
  auto phiNode =
      TheBuilder->CreatePHI(Type::getDoubleTy(*TheContext), 2, "iftmp");
  phiNode->addIncoming(thenValue, thenBB);
  phiNode->addIncoming(elseValue, elseBB);
  return phiNode;
}

Value *ForExprAST::codegen() {
  auto start = start_->codegen();

  // Create block for condition.
  auto fn = TheBuilder->GetInsertBlock()->getParent();
  auto preheaderBB = TheBuilder->GetInsertBlock();
  auto loopBB = BasicBlock::Create(*TheContext, "loop", fn);
  TheBuilder->CreateBr(loopBB);

  TheBuilder->SetInsertPoint(loopBB);
  auto varPHI =
      TheBuilder->CreatePHI(Type::getDoubleTy(*TheContext), 2, varName_);
  varPHI->addIncoming(start, preheaderBB);

  // Store variable name in environment temporarily when doing codegen for body.
  auto oldVal = NamedValues[varName_];
  NamedValues[varName_] = varPHI;

  auto body = body_->codegen();
  auto step =
      step_ ? step_->codegen() : ConstantFP::get(*TheContext, APFloat(1.0));
  auto nextVar = TheBuilder->CreateFAdd(varPHI, step);

  auto end = end_->codegen();
  end = TheBuilder->CreateFCmpONE(
      end, ConstantFP::get(*TheContext, APFloat(0.0)), "loopcond");

  auto loopEndBB = TheBuilder->GetInsertBlock();
  auto afterBB = BasicBlock::Create(*TheContext, "afterloop", fn);
  TheBuilder->CreateCondBr(end, loopBB, afterBB);

  TheBuilder->SetInsertPoint(afterBB);
  varPHI->addIncoming(nextVar, loopEndBB);

  // Restore old variable name in environment.
  if (oldVal) {
    NamedValues[varName_] = oldVal;
  } else {
    NamedValues.erase(varName_);
  }

  return Constant::getNullValue(Type::getDoubleTy(*TheContext));
}

Value *CallExprAST::codegen() {
  auto fn = getFunction(callee_);
  if (!fn) {
    throw CodegenException("Function " + callee_ +
                           " can't be found in the module");
  }
  if (fn->arg_size() != args_.size()) {
    throw CodegenException("Incorrect # of args passed");
  }

  std::vector<Value *> args;
  for (auto &arg : args_) {
    args.push_back(arg->codegen());
  }
  return TheBuilder->CreateCall(fn, args);
}

Function *PrototypeAST::codegen() {
  std::vector<Type *> argTypes(args_.size(), Type::getDoubleTy(*TheContext));
  auto functionType =
      FunctionType::get(Type::getDoubleTy(*TheContext), argTypes, false);
  // Can do this or:
  // auto fn = dyn_cast<Function>(TheModule->getOrInsertFunction(name_,
  // functionType).getCallee());
  auto fn = Function::Create(functionType, Function::ExternalLinkage, name_,
                             *TheModule);
  unsigned i = 0;
  for (auto &arg : fn->args()) {
    arg.setName(args_[i++]);
  }
  return fn;
}

Function *FunctionAST::codegen() {
  auto &proto = *prototype_;
  FunctionProtos[prototype_->getName()] = std::move(prototype_);
  auto fn = getFunction(proto.getName());
  if (!fn) {
    throw CodegenException("Could not find function");
  }
  if (!fn->empty()) {
    throw CodegenException("Function cannot be redefined");
  }

  auto BB = BasicBlock::Create(*TheContext, "entry", fn);
  TheBuilder->SetInsertPoint(BB);

  NamedValues.clear();
  for (auto &arg : fn->args()) {
    NamedValues[arg.getName()] = &arg;
  }

  try {
    auto result = body_->codegen();
    TheBuilder->CreateRet(result);
    verifyFunction(*fn);
    TheFPM->run(*fn, *TheFAM);
  } catch (CodegenException e) {
    fn->eraseFromParent();
    throw e;
  }
  return fn;
}