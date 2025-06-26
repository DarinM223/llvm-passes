#ifndef LEXER_H
#define LEXER_H

#include <string>

enum class Token {
  Eof = -1,
  Def = -2,
  Extern = -3,
  Identifier = -4,
  Number = -5
};

class Lexer {
  std::istream &in_;
  int lastChar_;
  std::string identifier_;
  double numberValue_;

public:
  Lexer(std::istream &in) : in_(in), lastChar_(' ') {}
  const std::string &getIdentifier() const { return identifier_; }
  double getNumber() const { return numberValue_; }
  Token getTok();
};

#endif