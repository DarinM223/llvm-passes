#include <iostream>

/*
 * To test this file, first run kaleidoscope with the --compile flag (to compile
 * to object code):
 * ```
 * ./kaleidoscope-exe --compile
 * ready> def average(x, y) (x + y) * 0.5;
 * ^D
 * ```
 *
 * Then run clang linking the object file:
 * ```
 * clang++-19 test_object_code.cpp output.o -o main
 * ./main
 * ```
 */

extern "C" {
double average(double, double);
}

int main() {
  std::cout << "average of 3.0 and 4.0: " << average(3.0, 4.0) << std::endl;
}
