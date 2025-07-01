#ifndef LIBRARY_H
#define LIBRARY_H

#include <cstdio>

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

extern "C" DLLEXPORT double putchard(double x) {
  fputc((char)x, stderr);
  return 0;
}

extern "C" DLLEXPORT double printd(double x) {
  fprintf(stderr, "%f\n", x);
  return 0;
}

#endif