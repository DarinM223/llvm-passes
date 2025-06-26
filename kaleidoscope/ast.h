#ifndef AST_H
#define AST_H

#include <memory>
#include <string>
#include <vector>

class ExprAST {
public:
  virtual ~ExprAST() = default;
};

class NumberExprAST : public ExprAST {
  double val_;

public:
  NumberExprAST(double val) : val_(val) {}
};

class VariableExprAST : public ExprAST {
  std::string name_;

public:
  VariableExprAST(const std::string &name) : name_(name) {}
};

class BinaryExprAST : public ExprAST {
  char op_;
  std::unique_ptr<ExprAST> lhs_, rhs_;

public:
  BinaryExprAST(char op, std::unique_ptr<ExprAST> lhs,
                std::unique_ptr<ExprAST> rhs)
      : op_(op), lhs_(std::move(lhs)), rhs_(std::move(rhs)) {}
};

class CallExprAST : public ExprAST {
  std::string callee_;
  std::vector<std::unique_ptr<ExprAST>> args_;

public:
  CallExprAST(const std::string &callee,
              std::vector<std::unique_ptr<ExprAST>> args)
      : callee_(callee), args_(std::move(args)) {}
};

class PrototypeAST {
  std::string name_;
  std::vector<std::string> args_;

public:
  PrototypeAST(const std::string &name, std::vector<std::string> args)
      : name_(name), args_(std::move(args)) {}

  const std::string &getName() const noexcept { return name_; }
};

class FunctionAST {
  std::unique_ptr<PrototypeAST> prototype_;
  std::unique_ptr<ExprAST> body_;

public:
  FunctionAST(std::unique_ptr<PrototypeAST> prototype,
              std::unique_ptr<ExprAST> body)
      : prototype_(std::move(prototype)), body_(std::move(body)) {}
};

#endif