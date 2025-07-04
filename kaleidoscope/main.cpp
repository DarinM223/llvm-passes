#include "lexer.h"
#include "parser.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include <iostream>
#include <unordered_map>

int main() {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();

  std::unordered_map<char, int> binopPrecedence;
  binopPrecedence['='] = 2;
  binopPrecedence['<'] = 10;
  binopPrecedence['+'] = 20;
  binopPrecedence['-'] = 20;
  binopPrecedence['*'] = 40;
  Lexer lexer(std::cin);
  Parser parser(lexer, std::move(binopPrecedence));
  Driver driver(std::cout, parser);
  driver.mainLoop();
  return 0;
}