llvm-passes
===========

To build and run:
-----------------

To build a bitcode `.bc` file, run `./build-bitcode.sh` on the relevant C file in the `inputs/` directory. Then in the root directory, run `cmake .` and `make` to build kaleidoscope and the various LLVM pass shared libraries.

To run cppcheck:
----------------

```
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON .
cppcheck --enable=all --suppress=missingInclude --suppress=missingIncludeSystem --project=compile_commands.json
```