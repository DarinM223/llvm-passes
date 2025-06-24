; ModuleID = 'input_for_fcmp_eq.c'
source_filename = "input_for_fcmp_eq.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

; Function Attrs: noinline nounwind uwtable
define dso_local double @sqrt_impl(double noundef %0, double noundef %1, double noundef %2) #0 {
  %4 = alloca double, align 8
  %5 = alloca double, align 8
  %6 = alloca double, align 8
  %7 = alloca double, align 8
  %8 = alloca double, align 8
  store double %0, ptr %5, align 8
  store double %1, ptr %6, align 8
  store double %2, ptr %7, align 8
  %9 = load double, ptr %6, align 8
  %10 = load double, ptr %7, align 8
  %11 = fcmp oeq double %9, %10
  br i1 %11, label %12, label %14

12:                                               ; preds = %3
  %13 = load double, ptr %7, align 8
  store double %13, ptr %4, align 8
  br label %36

14:                                               ; preds = %3
  %15 = load double, ptr %7, align 8
  %16 = load double, ptr %6, align 8
  %17 = fadd double %15, %16
  %18 = fadd double %17, 1.000000e+00
  %19 = fdiv double %18, 2.000000e+00
  store double %19, ptr %8, align 8
  %20 = load double, ptr %5, align 8
  %21 = load double, ptr %8, align 8
  %22 = fdiv double %20, %21
  %23 = load double, ptr %8, align 8
  %24 = fcmp olt double %22, %23
  br i1 %24, label %25, label %31

25:                                               ; preds = %14
  %26 = load double, ptr %5, align 8
  %27 = load double, ptr %7, align 8
  %28 = load double, ptr %8, align 8
  %29 = fsub double %28, 1.000000e+00
  %30 = call double @sqrt_impl(double noundef %26, double noundef %27, double noundef %29)
  store double %30, ptr %4, align 8
  br label %36

31:                                               ; preds = %14
  %32 = load double, ptr %5, align 8
  %33 = load double, ptr %8, align 8
  %34 = load double, ptr %6, align 8
  %35 = call double @sqrt_impl(double noundef %32, double noundef %33, double noundef %34)
  store double %35, ptr %4, align 8
  br label %36

36:                                               ; preds = %31, %25, %12
  %37 = load double, ptr %4, align 8
  ret double %37
}

; Function Attrs: noinline nounwind uwtable
define dso_local double @sqrt(double noundef %0) #0 {
  %2 = alloca double, align 8
  store double %0, ptr %2, align 8
  %3 = load double, ptr %2, align 8
  %4 = load double, ptr %2, align 8
  %5 = fdiv double %4, 2.000000e+00
  %6 = fadd double %5, 1.000000e+00
  %7 = call double @sqrt_impl(double noundef %3, double noundef 0.000000e+00, double noundef %6)
  ret double %7
}

; Function Attrs: noinline nounwind uwtable
define dso_local i32 @main() #0 {
  %1 = alloca i32, align 4
  %2 = alloca double, align 8
  %3 = alloca double, align 8
  store i32 0, ptr %1, align 4
  store double 2.000000e-01, ptr %2, align 8
  %4 = call double @sqrt(double noundef 5.000000e+00) #1
  %5 = fdiv double 1.000000e+00, %4
  %6 = call double @sqrt(double noundef 5.000000e+00) #1
  %7 = fdiv double %5, %6
  store double %7, ptr %3, align 8
  %8 = load double, ptr %3, align 8
  %9 = fcmp oeq double %8, 1.000000e+00
  br i1 %9, label %10, label %16

10:                                               ; preds = %0
  %11 = load double, ptr %2, align 8
  %12 = load double, ptr %3, align 8
  %13 = fcmp oeq double %11, %12
  %14 = zext i1 %13 to i64
  %15 = select i1 %13, i32 1, i32 0
  store i32 %15, ptr %1, align 4
  br label %22

16:                                               ; preds = %0
  %17 = load double, ptr %2, align 8
  %18 = load double, ptr %3, align 8
  %19 = fcmp oeq double %17, %18
  %20 = zext i1 %19 to i64
  %21 = select i1 %19, i32 0, i32 1
  store i32 %21, ptr %1, align 4
  br label %22

22:                                               ; preds = %16, %10
  %23 = load i32, ptr %1, align 4
  ret i32 %23
}

attributes #0 = { noinline nounwind uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nounwind }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"Ubuntu clang version 19.1.1 (1ubuntu1~24.04.2)"}
