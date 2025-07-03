#include "parser.h"
#include "library.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_os_ostream.h"
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

std::unique_ptr<ExprAST> Parser::parseIfExpr() {
  getNextToken();

  auto Cond = parseExpression();
  if (static_cast<Token>(currentToken_) != Token::Then) {
    throw ParserException("Expected then");
  }
  getNextToken();

  auto Then = parseExpression();
  if (static_cast<Token>(currentToken_) != Token::Else) {
    throw ParserException("Expected else");
  }
  getNextToken();

  auto Else = parseExpression();
  return std::make_unique<IfExprAST>(std::move(Cond), std::move(Then),
                                     std::move(Else));
}

std::unique_ptr<ExprAST> Parser::parseForExpr() {
  getNextToken();

  if (static_cast<Token>(currentToken_) != Token::Identifier) {
    throw ParserException("Expected identifier after for");
  }
  std::string idName(lexer_.getIdentifier());
  getNextToken();

  if (currentToken_ != '=') {
    throw ParserException("Expected '=' after for");
  }
  getNextToken();

  auto start = parseExpression();
  if (currentToken_ != ',') {
    throw ParserException("Expected ',' after for start value");
  }
  getNextToken();

  auto end = parseExpression();

  // The step value is optional
  std::unique_ptr<ExprAST> step;
  if (currentToken_ == ',') {
    getNextToken();
    step = parseExpression();
  }

  if (static_cast<Token>(currentToken_) != Token::In) {
    throw ParserException("Expected 'in' after for");
  }
  getNextToken();

  auto body = parseExpression();
  return std::make_unique<ForExprAST>(idName, std::move(start), std::move(end),
                                      std::move(step), std::move(body));
}

std::unique_ptr<ExprAST> Parser::parsePrimary() {
  switch (static_cast<Token>(currentToken_)) {
  case Token::Number:
    return parseNumberExpr();
  case Token::Identifier:
    return parseIdentifierExpr();
  case static_cast<Token>('('):
    return parseParenExpr();
  case Token::If:
    return parseIfExpr();
  case Token::For:
    return parseForExpr();
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
  auto lhs = parseUnary();
  return parseBinOpRHS(0, std::move(lhs));
}

std::unique_ptr<ExprAST> Parser::parseUnary() {
  if (!isascii(currentToken_) || currentToken_ == '(' || currentToken_ == ',') {
    return parsePrimary();
  }

  char opc = currentToken_;
  getNextToken();
  auto operand = parseUnary();
  return std::make_unique<UnaryExprAST>(opc, std::move(operand));
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

    auto rhs = parseUnary();
    int nextPrec = getTokPrecedence();
    if (tokPrec < nextPrec) {
      rhs = parseBinOpRHS(tokPrec + 1, std::move(rhs));
    }
    lhs =
        std::make_unique<BinaryExprAST>(binOp, std::move(lhs), std::move(rhs));
  }
  assert(0 && "parseBinOpRHS: cannot reach here");
}

enum class ParsePrototypeType { Identifier = 0, Unary, Binary };

std::unique_ptr<PrototypeAST> Parser::parsePrototype() {
  std::string fnName;
  auto kind = ParsePrototypeType::Identifier;
  unsigned binaryPrecedence = 30;

  switch (static_cast<Token>(currentToken_)) {
  case Token::Identifier:
    fnName = lexer_.getIdentifier();
    getNextToken();
    break;
  case Token::Unary:
    getNextToken();
    if (!isascii(currentToken_)) {
      throw ParserException("Expected unary operator");
    }
    fnName = std::string("unary") + static_cast<char>(currentToken_);
    kind = ParsePrototypeType::Unary;
    getNextToken();
    break;
  case Token::Binary:
    getNextToken();
    if (!isascii(currentToken_)) {
      throw ParserException("Expected binary operator");
    }
    fnName = std::string("binary") + static_cast<char>(currentToken_);
    kind = ParsePrototypeType::Binary;
    getNextToken();
    if (static_cast<Token>(currentToken_) == Token::Number) {
      if (lexer_.getNumber() < 1 || lexer_.getNumber() > 100) {
        throw ParserException("Invalid precedence: must be 1..100");
      }
      binaryPrecedence = (unsigned)lexer_.getNumber();
      getNextToken();
    }
    break;
  default:
    throw ParserException("Expected function name in prototype");
  }

  if (currentToken_ != '(') {
    throw ParserException("Expected '(' in prototype");
  }

  std::vector<std::string> argNames;
  while (static_cast<Token>(getNextToken()) == Token::Identifier) {
    argNames.emplace_back(lexer_.getIdentifier());
    getNextToken();
    if (currentToken_ == ')') {
      break;
    }
    if (currentToken_ != ',') {
      throw ParserException("Expected ')' or ',' in argument list");
    }
  }
  if (currentToken_ != ')') {
    throw ParserException("Expected ')' in prototype");
  }
  getNextToken();

  bool isOperator = kind != ParsePrototypeType::Identifier;
  if (isOperator && argNames.size() != static_cast<size_t>(kind)) {
    throw ParserException("Invalid number of operands for operator");
  }

  return std::make_unique<PrototypeAST>(fnName, std::move(argNames), isOperator,
                                        binaryPrecedence);
}

std::unique_ptr<FunctionAST> Parser::parseDefinition() {
  getNextToken();
  auto proto = parsePrototype();
  auto expr = parseExpression();
  return std::make_unique<FunctionAST>(std::move(proto), std::move(expr));
}

std::unique_ptr<FunctionAST> Parser::parseTopLevelExpr() {
  auto expr = parseExpression();
  auto proto =
      std::make_unique<PrototypeAST>("__anon_expr", std::vector<std::string>());
  return std::make_unique<FunctionAST>(std::move(proto), std::move(expr));
}

std::unique_ptr<PrototypeAST> Parser::parseExtern() {
  getNextToken();
  return parsePrototype();
}

Driver::Driver(std::ostream &out, Parser &parser) : out_(out), parser_(parser) {
  jit_ = ExitOnErr(llvm::orc::KaleidoscopeJIT::Create());
  ExitOnErr(jit_->addSymbols(
      {{"putchard", (void *)&putchard}, {"printd", (void *)&printd}}));
  initializeModuleAndManagers(jit_->getDataLayout());
}

void Driver::handleDefinition() {
  try {
    auto ast = parser_.parseDefinition();
    auto ir = ast->codegen(parser_.binopPrecedence_);
    out_ << "Read function definition: ";
    llvm::raw_os_ostream rout(out_);
    ir->print(rout);
    out_ << "\n";
    ExitOnErr(jit_->addModule(llvm::orc::ThreadSafeModule(
        std::move(TheModule), std::move(TheContext))));
    initializeModuleAndManagers(jit_->getDataLayout());
  } catch (ParserException e) {
    out_ << "Error: " << e.what() << "\n";
    parser_.getNextToken();
  } catch (CodegenException e) {
    out_ << "Error: " << e.what() << "\n";
  }
}

void Driver::handleExtern() {
  try {
    auto ast = parser_.parseExtern();
    auto ir = ast->codegen();
    out_ << "Read extern: ";
    llvm::raw_os_ostream rout(out_);
    ir->print(rout);
    out_ << "\n";
    FunctionProtos[ast->getName()] = std::move(ast);
  } catch (ParserException e) {
    out_ << "Error: " << e.what() << "\n";
    parser_.getNextToken();
  } catch (CodegenException e) {
    out_ << "Error: " << e.what() << "\n";
  }
}

void Driver::handleTopLevelExpression() {
  try {
    auto ast = parser_.parseTopLevelExpr();
    auto ir = ast->codegen(parser_.binopPrecedence_);
    out_ << "Read top-level expr: ";
    llvm::raw_os_ostream rout(out_);
    ir->print(rout);
    out_ << "\n";

    auto rt = jit_->getMainJITDylib().createResourceTracker();
    auto tsm = llvm::orc::ThreadSafeModule(std::move(TheModule),
                                           std::move(TheContext));
    ExitOnErr(jit_->addModule(std::move(tsm), rt));
    initializeModuleAndManagers(jit_->getDataLayout());

    auto exprSymbol = ExitOnErr(jit_->lookup("__anon_expr"));
    double (*FP)() = exprSymbol.getAddress().toPtr<double (*)()>();
    out_ << "Evaluated to: " << FP() << "\n";

    ExitOnErr(rt->remove());
  } catch (ParserException e) {
    out_ << "Error: " << e.what() << "\n";
    parser_.getNextToken();
  } catch (CodegenException e) {
    out_ << "Error: " << e.what() << "\n";
  }
}

void Driver::mainLoop() {
  out_ << "ready> ";
  parser_.getNextToken();
  while (true) {
    out_ << "ready> ";
    switch (parser_.getCurrentToken()) {
    case Token::Eof:
      return;
    case static_cast<Token>(';'):
      parser_.getNextToken();
      break;
    case Token::Def:
      handleDefinition();
      break;
    case Token::Extern:
      handleExtern();
      break;
    default:
      handleTopLevelExpression();
      break;
    }
  }
}