#include "lexer.h"
#include "parser.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/TargetParser/Host.h"
#include <cstring>
#include <iostream>
#include <unordered_map>

int main(int argc, char **argv) {
  bool useJIT = true;
  if (argc > 1 && strcmp(argv[1], "--compile") == 0) {
    useJIT = false;
  }

  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmParsers();
  llvm::InitializeAllAsmPrinters();

  std::unordered_map<char, int> binopPrecedence;
  binopPrecedence['='] = 2;
  binopPrecedence['<'] = 10;
  binopPrecedence['+'] = 20;
  binopPrecedence['-'] = 20;
  binopPrecedence['*'] = 40;
  Lexer lexer(std::cin);
  Parser parser(lexer, std::move(binopPrecedence));
  Driver driver(std::cout, parser, useJIT);
  driver.mainLoop();

  // Exit if in JIT mode (can't compile to object code).
  if (useJIT) {
    return 0;
  }

  auto TargetTriple = llvm::sys::getDefaultTargetTriple();
  TheModule->setTargetTriple(TargetTriple);

  std::string err;
  auto Target = llvm::TargetRegistry::lookupTarget(TargetTriple, err);
  if (!Target) {
    llvm::errs() << err;
    return 1;
  }

  auto CPU = "generic";
  auto Features = "";

  llvm::TargetOptions opt;
  // Put this in a unique_ptr or else it leaks memory.
  std::unique_ptr<llvm::TargetMachine> TheTargetMachine(
      Target->createTargetMachine(TargetTriple, CPU, Features, opt,
                                  llvm::Reloc::PIC_));

  TheModule->setDataLayout(TheTargetMachine->createDataLayout());

  auto Filename = "output.o";
  std::error_code EC;
  llvm::raw_fd_ostream dest(Filename, EC, llvm::sys::fs::OF_None);
  if (EC) {
    llvm::errs() << "Could not open file " << EC.message();
    return 1;
  }

  llvm::legacy::PassManager manager;
  auto FileType = llvm::CodeGenFileType::ObjectFile;

  if (TheTargetMachine->addPassesToEmitFile(manager, dest, nullptr, FileType)) {
    llvm::errs() << "TheTargetMachine can't emit a file of this type";
    return 1;
  }

  manager.run(*TheModule);
  dest.flush();

  llvm::outs() << "Wrote " << Filename << "\n";
  return 0;
}