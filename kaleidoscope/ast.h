#ifndef AST_H
#define AST_H

#include "llvm/Analysis/CGSCCPassManager.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassInstrumentation.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Value.h"
#include "llvm/Passes/StandardInstrumentations.h"
#include <memory>
#include <string>
#include <vector>

class PrototypeAST;

extern std::unique_ptr<llvm::LLVMContext> TheContext;
extern std::unique_ptr<llvm::IRBuilder<>> TheBuilder;
extern std::unique_ptr<llvm::Module> TheModule;
extern llvm::StringMap<llvm::Value *> NamedValues;
extern llvm::StringMap<std::unique_ptr<PrototypeAST>> FunctionProtos;

extern std::unique_ptr<llvm::FunctionPassManager> TheFPM;
extern std::unique_ptr<llvm::LoopAnalysisManager> TheLAM;
extern std::unique_ptr<llvm::FunctionAnalysisManager> TheFAM;
extern std::unique_ptr<llvm::CGSCCAnalysisManager> TheCGAM;
extern std::unique_ptr<llvm::ModuleAnalysisManager> TheMAM;
extern std::unique_ptr<llvm::PassInstrumentationCallbacks> ThePIC;
extern std::unique_ptr<llvm::StandardInstrumentations> TheSI;

void initializeModuleAndManagers(const llvm::DataLayout &layout);

class ExprAST {
public:
  virtual ~ExprAST() = default;
  virtual llvm::Value *codegen() = 0;
};

class NumberExprAST : public ExprAST {
  double val_;

public:
  NumberExprAST(double val) : val_(val) {}
  llvm::Value *codegen() override;
};

class VariableExprAST : public ExprAST {
  std::string name_;

public:
  VariableExprAST(const std::string &name) : name_(name) {}
  llvm::Value *codegen() override;
};

class BinaryExprAST : public ExprAST {
  char op_;
  std::unique_ptr<ExprAST> lhs_, rhs_;

public:
  BinaryExprAST(char op, std::unique_ptr<ExprAST> lhs,
                std::unique_ptr<ExprAST> rhs)
      : op_(op), lhs_(std::move(lhs)), rhs_(std::move(rhs)) {}
  llvm::Value *codegen() override;
};

class CallExprAST : public ExprAST {
  std::string callee_;
  std::vector<std::unique_ptr<ExprAST>> args_;

public:
  CallExprAST(const std::string &callee,
              std::vector<std::unique_ptr<ExprAST>> args)
      : callee_(callee), args_(std::move(args)) {}
  llvm::Value *codegen() override;
};

class PrototypeAST {
  std::string name_;
  std::vector<std::string> args_;

public:
  PrototypeAST(const std::string &name, std::vector<std::string> args)
      : name_(name), args_(std::move(args)) {}

  const std::string &getName() const noexcept { return name_; }
  llvm::Function *codegen();
};

class FunctionAST {
  std::unique_ptr<PrototypeAST> prototype_;
  std::unique_ptr<ExprAST> body_;

public:
  FunctionAST(std::unique_ptr<PrototypeAST> prototype,
              std::unique_ptr<ExprAST> body)
      : prototype_(std::move(prototype)), body_(std::move(body)) {}
  llvm::Function *codegen();
};

class CodegenException : public std::exception {
  std::string reason_;

public:
  CodegenException(const std::string &reason) : reason_(reason) {}
  virtual const char *what() const throw() { return reason_.c_str(); }
};

#endif