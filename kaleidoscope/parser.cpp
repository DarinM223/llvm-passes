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

std::unique_ptr<PrototypeAST> Parser::parsePrototype() {
  if (static_cast<Token>(currentToken_) != Token::Identifier) {
    throw ParserException("Expected function name in prototype");
  }
  std::string fnName(lexer_.getIdentifier());
  getNextToken();
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

  return std::make_unique<PrototypeAST>(fnName, std::move(argNames));
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
  llvm::cantFail(jit_->addSymbols(
      {{"putchard", (void *)&putchard}, {"printd", (void *)&printd}}));
  initializeModuleAndManagers(jit_->getDataLayout());
}

void Driver::handleDefinition() {
  try {
    auto ast = parser_.parseDefinition();
    auto ir = ast->codegen();
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
    auto ir = ast->codegen();
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