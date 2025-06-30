#include "ast.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Scalar/Reassociate.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"
#include <ranges>

using namespace llvm;

static std::unique_ptr<LLVMContext> TheContext;
static std::unique_ptr<IRBuilder<>> TheBuilder;
static std::unique_ptr<Module> TheModule;
static StringMap<Value *> NamedValues;

void initializeModuleAndManagers() {
  TheContext = std::make_unique<LLVMContext>();
  TheBuilder = std::make_unique<IRBuilder<>>(*TheContext);
  TheModule = std::make_unique<Module>("my cool jit", *TheContext);
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

Value *CallExprAST::codegen() {
  auto fn = TheModule->getFunction(callee_);
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
  auto fn = TheModule->getFunction(prototype_->getName());
  if (!fn) {
    fn = prototype_->codegen();
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