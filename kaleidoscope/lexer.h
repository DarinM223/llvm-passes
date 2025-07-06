#ifndef LEXER_H
#define LEXER_H

#include <string>

enum class Token {
  Eof = -1,
  Def = -2,
  Extern = -3,
  Identifier = -4,
  Number = -5,
  If = -6,
  Then = -7,
  Else = -8,
  For = -9,
  In = -10,
  Binary = -11,
  Unary = -12,
  Var = -13
};

class Lexer {
  std::istream &in_;
  int lastChar_;
  std::string identifier_;
  double numberValue_;

public:
  explicit Lexer(std::istream &in) : in_(in), lastChar_(' '), numberValue_(0) {}
  const std::string &getIdentifier() const { return identifier_; }
  double getNumber() const { return numberValue_; }
  Token getTok();
};

#endif