#include "lexer.h"
#include <iostream>

Token Lexer::getTok() {
  while (isspace(lastChar_)) {
    lastChar_ = in_.get();
  }

  if (isalpha(lastChar_)) {
    identifier_.assign(1, lastChar_);
    while (isalnum((lastChar_ = in_.get()))) {
      identifier_ += lastChar_;
    }
    if (identifier_ == "def") {
      return Token::Def;
    }
    if (identifier_ == "extern") {
      return Token::Extern;
    }
    return Token::Identifier;
  }
  if (isdigit(lastChar_) || lastChar_ == '.') {
    std::string numStr;
    do {
      numStr += lastChar_;
      lastChar_ = in_.get();
    } while (isdigit(lastChar_) || lastChar_ == '.');
    numberValue_ = strtod(numStr.c_str(), 0);
    return Token::Number;
  }
  if (lastChar_ == '#') {
    do {
      lastChar_ = in_.get();
    } while (lastChar_ != EOF && lastChar_ != '\n' && lastChar_ != '\r');
    if (lastChar_ != EOF) {
      return getTok();
    }
  }
  if (lastChar_ == EOF) {
    return Token::Eof;
  }

  Token tok(static_cast<Token>(lastChar_));
  lastChar_ = in_.get();
  return tok;
}