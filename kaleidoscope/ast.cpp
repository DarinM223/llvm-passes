#include "ast.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Scalar/Reassociate.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"
#include "llvm/Transforms/Utils/Mem2Reg.h"
#include <ranges>

using namespace llvm;

std::unique_ptr<llvm::LLVMContext> TheContext;
std::unique_ptr<llvm::IRBuilder<>> TheBuilder;
std::unique_ptr<llvm::Module> TheModule;
llvm::StringMap<llvm::AllocaInst *> NamedValues;
llvm::StringMap<std::unique_ptr<PrototypeAST>> FunctionProtos;

std::unique_ptr<llvm::FunctionPassManager> TheFPM;
std::unique_ptr<llvm::LoopAnalysisManager> TheLAM;
std::unique_ptr<llvm::FunctionAnalysisManager> TheFAM;
std::unique_ptr<llvm::CGSCCAnalysisManager> TheCGAM;
std::unique_ptr<llvm::ModuleAnalysisManager> TheMAM;
std::unique_ptr<llvm::PassInstrumentationCallbacks> ThePIC;
std::unique_ptr<llvm::StandardInstrumentations> TheSI;

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

  TheFPM->addPass(PromotePass());
  TheFPM->addPass(InstCombinePass());
  TheFPM->addPass(ReassociatePass());
  TheFPM->addPass(GVNPass());
  TheFPM->addPass(SimplifyCFGPass());

  PassBuilder PB;
  PB.registerModuleAnalyses(*TheMAM);
  PB.registerFunctionAnalyses(*TheFAM);
  PB.crossRegisterProxies(*TheLAM, *TheFAM, *TheCGAM, *TheMAM);
}

void initializeModuleAndManagers(const DataLayout &layout) {
  initializeModuleAndManagers();
  TheModule->setDataLayout(layout);
}

static AllocaInst *createEntryBlockAlloca(Function *fn, StringRef varName) {
  IRBuilder<> builder(&fn->getEntryBlock(), fn->getEntryBlock().begin());
  return builder.CreateAlloca(Type::getDoubleTy(fn->getContext()), nullptr,
                              varName);
}

Function *getFunction(StringRef name) {
  if (auto fn = TheModule->getFunction(name)) {
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
  auto alloca = NamedValues.lookup(name_);
  return TheBuilder->CreateLoad(alloca->getAllocatedType(), alloca, name_);
}

Value *VarExprAST::codegen() {
  StringMap<llvm::AllocaInst *> oldValues;
  auto fn = TheBuilder->GetInsertBlock()->getParent();
  for (auto &[var, init] : varNames_) {
    auto initVal =
        init ? init->codegen() : ConstantFP::get(*TheContext, APFloat(0.0));
    auto alloca = createEntryBlockAlloca(fn, var);
    TheBuilder->CreateStore(initVal, alloca);
    oldValues[var] = NamedValues[var];
    NamedValues[var] = alloca;
  }

  auto result = body_->codegen();

  for (auto &[var, _] : varNames_) {
    if (oldValues[var]) {
      NamedValues[var] = oldValues[var];
    } else {
      NamedValues.erase(var);
    }
  }
  return result;
}

Value *UnaryExprAST::codegen() {
  auto operand = operand_->codegen();
  auto unOpName = std::string("unary") + op_;
  auto fn = getFunction(unOpName);
  if (!fn) {
    throw CodegenException("Unary operator " + unOpName + "not found!");
  }
  return TheBuilder->CreateCall(fn, operand, "unop");
}

Value *BinaryExprAST::codegen() {
  if (op_ == '=') {
    auto lhse = static_cast<VariableExprAST *>(lhs_.get());
    if (!lhse) {
      throw CodegenException("Destination of '=' must be a variable");
    }

    auto rhs = rhs_->codegen();
    if (!NamedValues.contains(lhse->getName())) {
      throw CodegenException("Unknown variable name: " + lhse->getName());
    }
    auto alloca = NamedValues[lhse->getName()];
    TheBuilder->CreateStore(rhs, alloca);
    return rhs;
  }

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
    break;
  }

  // Emit calls for user defined operators
  std::string binOpName = std::string("binary") + op_;
  Function *fn = getFunction(binOpName);
  if (!fn) {
    throw CodegenException("Binary operator " + binOpName + " not found!");
  }

  return TheBuilder->CreateCall(fn, {lhs, rhs}, "binop");
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
  auto fn = TheBuilder->GetInsertBlock()->getParent();
  auto alloca = createEntryBlockAlloca(fn, varName_);
  auto start = start_->codegen();
  TheBuilder->CreateStore(start, alloca);

  // Create block for condition.
  auto loopBB = BasicBlock::Create(*TheContext, "loop", fn);
  TheBuilder->CreateBr(loopBB);
  TheBuilder->SetInsertPoint(loopBB);

  // Store variable name in environment temporarily when doing codegen for body.
  auto oldVal = NamedValues[varName_];
  NamedValues[varName_] = alloca;

  body_->codegen();
  auto step =
      step_ ? step_->codegen() : ConstantFP::get(*TheContext, APFloat(1.0));
  auto end = end_->codegen();

  auto nextVar = TheBuilder->CreateFAdd(
      TheBuilder->CreateLoad(alloca->getAllocatedType(), alloca, varName_),
      step);
  TheBuilder->CreateStore(nextVar, alloca);

  end = TheBuilder->CreateFCmpONE(
      end, ConstantFP::get(*TheContext, APFloat(0.0)), "loopcond");

  auto afterBB = BasicBlock::Create(*TheContext, "afterloop", fn);
  TheBuilder->CreateCondBr(end, loopBB, afterBB);
  TheBuilder->SetInsertPoint(afterBB);

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

Function *FunctionAST::codegen(std::unordered_map<char, int> &binopPrecedence) {
  const auto &proto = *prototype_;
  FunctionProtos[prototype_->getName()] = std::move(prototype_);
  auto fn = getFunction(proto.getName());
  if (!fn) {
    throw CodegenException("Could not find function");
  }
  if (!fn->empty()) {
    throw CodegenException("Function cannot be redefined");
  }

  if (proto.isBinaryOp()) {
    binopPrecedence[proto.getOperatorName()] = proto.getBinaryPrecedence();
  }

  auto BB = BasicBlock::Create(*TheContext, "entry", fn);
  TheBuilder->SetInsertPoint(BB);

  NamedValues.clear();
  for (auto &arg : fn->args()) {
    auto alloca = createEntryBlockAlloca(fn, arg.getName());
    TheBuilder->CreateStore(&arg, alloca);
    NamedValues[arg.getName()] = alloca;
  }

  try {
    auto result = body_->codegen();
    TheBuilder->CreateRet(result);
    verifyFunction(*fn);
    TheFPM->run(*fn, *TheFAM);
  } catch (CodegenException &e) {
    fn->eraseFromParent();
    throw;
  }
  return fn;
}