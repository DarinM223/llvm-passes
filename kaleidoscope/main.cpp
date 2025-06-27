#include "lexer.h"
#include "parser.h"
#include <iostream>
#include <unordered_map>

int main() {
  std::unordered_map<char, int> binopPrecedence;
  binopPrecedence['<'] = 10;
  binopPrecedence['+'] = 20;
  binopPrecedence['-'] = 20;
  binopPrecedence['*'] = 40;
  Lexer lexer(std::cin);
  Parser parser(lexer, std::move(binopPrecedence));
  Driver driver(std::cout, parser);

  parser.getNextToken();
  driver.mainLoop();
  return 0;
}