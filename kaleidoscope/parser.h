#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "lexer.h"
#include <iostream>
#include <unordered_map>

class Parser {
  Lexer &lexer_;
  int currentToken_;
  std::unordered_map<char, int> binopPrecedence_;

public:
  Parser(Lexer &lexer, std::unordered_map<char, int> binopPrecedence)
      : lexer_(lexer), binopPrecedence_(std::move(binopPrecedence)) {}
  Token getCurrentToken() { return static_cast<Token>(currentToken_); }
  int getNextToken() {
    return currentToken_ = static_cast<int>(lexer_.getTok());
  }
  int getTokPrecedence();
  std::unique_ptr<ExprAST> parseNumberExpr();
  std::unique_ptr<ExprAST> parseParenExpr();
  std::unique_ptr<ExprAST> parseIdentifierExpr();
  std::unique_ptr<ExprAST> parsePrimary();
  std::unique_ptr<ExprAST> parseExpression();
  std::unique_ptr<ExprAST> parseBinOpRHS(int prec,
                                         std::unique_ptr<ExprAST> lhs);
  std::unique_ptr<PrototypeAST> parsePrototype();
  std::unique_ptr<FunctionAST> parseDefinition();
  std::unique_ptr<FunctionAST> parseTopLevelExpr();
  std::unique_ptr<PrototypeAST> parseExtern();
};

class Driver {
  std::ostream &out_;
  Parser &parser_;

public:
  Driver(std::ostream &out, Parser &parser) : out_(out), parser_(parser) {}
  void handleDefinition();
  void handleExtern();
  void handleTopLevelExpression();
  void mainLoop();
};

class ParserException : public std::exception {
  std::string reason_;

public:
  ParserException(const std::string &reason) : reason_(reason) {}
  virtual const char *what() const throw() { return reason_.c_str(); }
};

#endif