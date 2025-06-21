if [[ -v DEBUG ]]; then
  gdb --args opt-19 -load-pass-plugin ./libRIV.so -load-pass-plugin ./libDuplicateBB.so -passes=duplicate-bb -S inputs/input_for_duplicate_bb.ll -o out.ll
else
  opt-19 -load-pass-plugin ./libRIV.so -load-pass-plugin ./libDuplicateBB.so -passes=duplicate-bb -S inputs/input_for_duplicate_bb.ll -o out.ll
fi