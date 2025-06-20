; ModuleID = 'input_for_riv.c'
source_filename = "input_for_riv.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn memory(none) uwtable
define dso_local i32 @foo(i32 noundef %0, i32 noundef %1, i32 noundef %2) local_unnamed_addr #0 {
  %4 = add nsw i32 %0, 123
  %5 = icmp sgt i32 %0, 0
  br i1 %5, label %6, label %17

6:                                                ; preds = %3
  %7 = mul nsw i32 %1, %0
  %8 = sdiv i32 %1, %2
  %9 = icmp eq i32 %7, %8
  br i1 %9, label %10, label %14

10:                                               ; preds = %6
  %11 = mul i32 %7, -2
  %12 = mul i32 %11, %8
  %13 = add i32 %4, %12
  br label %17

14:                                               ; preds = %6
  %15 = mul nsw i32 %2, 987
  %16 = mul nsw i32 %15, %8
  br label %17

17:                                               ; preds = %3, %10, %14
  %18 = phi i32 [ %13, %10 ], [ %16, %14 ], [ 321, %3 ]
  ret i32 %18
}

attributes #0 = { mustprogress nofree norecurse nosync nounwind willreturn memory(none) uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.module.flags = !{!0, !1, !2, !3}
!llvm.ident = !{!4}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{!"Ubuntu clang version 19.1.1 (1ubuntu1~24.04.2)"}
