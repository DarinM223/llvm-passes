#include "parser.h"
#include <cassert>

std::unique_ptr<ExprAST> Parser::parseNumberExpr() {
  auto result = std::make_unique<NumberExprAST>(lexer_.getNumber());
  getNextToken();
  return result;
}

std::unique_ptr<ExprAST> Parser::parseParenExpr() {
  getNextToken();
  auto v = parseExpression();
  if (currentToken_ != ')') {
    throw ParserException("Expected ')'");
  }
  getNextToken();
  return v;
}

std::unique_ptr<ExprAST> Parser::parseIdentifierExpr() {
  std::string ident(lexer_.getIdentifier());
  getNextToken();
  // Variable
  if (currentToken_ != '(') {
    return std::make_unique<VariableExprAST>(ident);
  }

  // Call
  getNextToken();
  std::vector<std::unique_ptr<ExprAST>> args;
  if (currentToken_ != ')') {
    while (true) {
      args.push_back(parseExpression());
      if (currentToken_ == ')') {
        break;
      }
      if (currentToken_ != ',') {
        throw ParserException("Expected ')' or ',' in argument list");
      }
      getNextToken();
    }
  }

  getNextToken();
  return std::make_unique<CallExprAST>(ident, std::move(args));
}

std::unique_ptr<ExprAST> Parser::parsePrimary() {
  switch (static_cast<Token>(currentToken_)) {
  case Token::Number:
    return parseNumberExpr();
  case Token::Identifier:
    return parseIdentifierExpr();
  case static_cast<Token>('('):
    return parseParenExpr();
  default:
    throw ParserException("Unknown token when expecting an expression");
  }
}

int Parser::getTokPrecedence() {
  if (!isascii(currentToken_)) {
    return -1;
  }

  int tokPrec = binopPrecedence_[currentToken_];
  if (tokPrec <= 0) {
    return -1;
  }
  return tokPrec;
}

std::unique_ptr<ExprAST> Parser::parseExpression() {
  auto lhs = parsePrimary();
  return parseBinOpRHS(0, std::move(lhs));
}

std::unique_ptr<ExprAST> Parser::parseBinOpRHS(int prec,
                                               std::unique_ptr<ExprAST> lhs) {
  while (true) {
    int tokPrec = getTokPrecedence();
    if (tokPrec < prec) {
      return lhs;
    }

    int binOp = currentToken_;
    getNextToken();

    auto rhs = parsePrimary();
    int nextPrec = getTokPrecedence();
    if (tokPrec < nextPrec) {
      rhs = parseBinOpRHS(tokPrec + 1, std::move(rhs));
    }
    lhs =
        std::make_unique<BinaryExprAST>(binOp, std::move(lhs), std::move(rhs));
  }
  assert(0 && "parseBinOpRHS: cannot reach here");
}