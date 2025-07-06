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
#include <unordered_map>
#include <vector>

class PrototypeAST;

extern std::unique_ptr<llvm::LLVMContext> TheContext;
extern std::unique_ptr<llvm::IRBuilder<>> TheBuilder;
extern std::unique_ptr<llvm::Module> TheModule;
extern llvm::StringMap<llvm::AllocaInst *> NamedValues;
extern llvm::StringMap<std::unique_ptr<PrototypeAST>> FunctionProtos;

extern std::unique_ptr<llvm::FunctionPassManager> TheFPM;
extern std::unique_ptr<llvm::LoopAnalysisManager> TheLAM;
extern std::unique_ptr<llvm::FunctionAnalysisManager> TheFAM;
extern std::unique_ptr<llvm::CGSCCAnalysisManager> TheCGAM;
extern std::unique_ptr<llvm::ModuleAnalysisManager> TheMAM;
extern std::unique_ptr<llvm::PassInstrumentationCallbacks> ThePIC;
extern std::unique_ptr<llvm::StandardInstrumentations> TheSI;

void initializeModuleAndManagers();
void initializeModuleAndManagers(const llvm::DataLayout &layout);

class ExprAST {
public:
  virtual ~ExprAST() = default;
  virtual llvm::Value *codegen() = 0;
};

class NumberExprAST : public ExprAST {
  double val_;

public:
  explicit NumberExprAST(double val) : val_(val) {}
  llvm::Value *codegen() override;
};

/// @brief Expression for referencing defined variables.
class VariableExprAST : public ExprAST {
  std::string name_;

public:
  const std::string &getName() { return name_; }
  explicit VariableExprAST(const std::string &name) : name_(name) {}
  llvm::Value *codegen() override;
};

/// @brief Expression for creating new locally defined variables.
class VarExprAST : public ExprAST {
  std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> varNames_;
  std::unique_ptr<ExprAST> body_;

public:
  VarExprAST(
      std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> varNames,
      std::unique_ptr<ExprAST> body)
      : varNames_(std::move(varNames)), body_(std::move(body)) {}
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

class UnaryExprAST : public ExprAST {
  char op_;
  std::unique_ptr<ExprAST> operand_;

public:
  UnaryExprAST(char op, std::unique_ptr<ExprAST> operand)
      : op_(op), operand_(std::move(operand)) {}
  llvm::Value *codegen() override;
};

class IfExprAST : public ExprAST {
  std::unique_ptr<ExprAST> cond_, then_, else_;

public:
  IfExprAST(std::unique_ptr<ExprAST> Cond, std::unique_ptr<ExprAST> Then,
            std::unique_ptr<ExprAST> Else)
      : cond_(std::move(Cond)), then_(std::move(Then)), else_(std::move(Else)) {
  }
  llvm::Value *codegen() override;
};

class ForExprAST : public ExprAST {
  std::string varName_;
  std::unique_ptr<ExprAST> start_, end_, step_, body_;

public:
  ForExprAST(const std::string &varName, std::unique_ptr<ExprAST> start,
             std::unique_ptr<ExprAST> end, std::unique_ptr<ExprAST> step,
             std::unique_ptr<ExprAST> body)
      : varName_(varName), start_(std::move(start)), end_(std::move(end)),
        step_(std::move(step)), body_(std::move(body)) {}
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
  bool isOperator_;
  unsigned precedence_;

public:
  PrototypeAST(const std::string &name, std::vector<std::string> args,
               bool isOperator = false, unsigned precedence = 0)
      : name_(name), args_(std::move(args)), isOperator_(isOperator),
        precedence_(precedence) {}

  const std::string &getName() const noexcept { return name_; }
  bool isUnaryOp() const noexcept { return isOperator_ && args_.size() == 1; }
  bool isBinaryOp() const noexcept { return isOperator_ && args_.size() == 2; }
  char getOperatorName() const {
    assert(isUnaryOp() || isBinaryOp());
    return name_[name_.size() - 1];
  }
  unsigned getBinaryPrecedence() const noexcept { return precedence_; }
  llvm::Function *codegen();
};

class FunctionAST {
  std::unique_ptr<PrototypeAST> prototype_;
  std::unique_ptr<ExprAST> body_;

public:
  FunctionAST(std::unique_ptr<PrototypeAST> prototype,
              std::unique_ptr<ExprAST> body)
      : prototype_(std::move(prototype)), body_(std::move(body)) {}
  llvm::Function *codegen(std::unordered_map<char, int> &binopPrecedence);
};

class CodegenException : public std::exception {
  std::string reason_;

public:
  explicit CodegenException(const std::string &reason) : reason_(reason) {}
  virtual const char *what() const throw() { return reason_.c_str(); }
};

#endif