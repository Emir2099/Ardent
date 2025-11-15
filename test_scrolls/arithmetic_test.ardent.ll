; ModuleID = 'ardent_module'
source_filename = "ardent_module"

%ArdentValue = type { i32, i64, i8, ptr, i32 }

define i32 @ardent_entry() {
entry:
  %print.tmp3 = alloca %ArdentValue, align 8
  %print.tmp2 = alloca %ArdentValue, align 8
  %print.tmp1 = alloca %ArdentValue, align 8
  %print.tmp = alloca %ArdentValue, align 8
  %concat.right = alloca %ArdentValue, align 8
  %concat.left = alloca %ArdentValue, align 8
  %plus.result = alloca %ArdentValue, align 8
  br i1 true, label %plus.num, label %plus.concat

plus.num:                                         ; preds = %entry
  %addtmp = call i64 @ardent_rt_add_i64(i64 6, i64 7)
  %0 = insertvalue %ArdentValue { i32 0, i64 undef, i8 undef, ptr undef, i32 undef }, i64 %addtmp, 1
  %1 = insertvalue %ArdentValue %0, i8 0, 2
  %2 = insertvalue %ArdentValue %1, ptr null, 3
  %3 = insertvalue %ArdentValue %2, i32 0, 4
  store %ArdentValue %3, ptr %plus.result, align 8
  br label %plus.merge

plus.concat:                                      ; preds = %entry
  store %ArdentValue { i32 0, i64 6, i8 0, ptr null, i32 0 }, ptr %concat.left, align 8
  store %ArdentValue { i32 0, i64 7, i8 0, ptr null, i32 0 }, ptr %concat.right, align 8
  call void @ardent_rt_concat_av_ptr(ptr %concat.left, ptr %concat.right, ptr %plus.result)
  br label %plus.merge

plus.merge:                                       ; preds = %plus.concat, %plus.num
  %plus.final = load %ArdentValue, ptr %plus.result, align 8
  store %ArdentValue %plus.final, ptr %print.tmp, align 8
  call void @ardent_rt_print_av_ptr(ptr %print.tmp)
  %subtmp = call i64 @ardent_rt_sub_i64(i64 10, i64 3)
  %4 = insertvalue %ArdentValue { i32 0, i64 undef, i8 undef, ptr undef, i32 undef }, i64 %subtmp, 1
  %5 = insertvalue %ArdentValue %4, i8 0, 2
  %6 = insertvalue %ArdentValue %5, ptr null, 3
  %7 = insertvalue %ArdentValue %6, i32 0, 4
  store %ArdentValue %7, ptr %print.tmp1, align 8
  call void @ardent_rt_print_av_ptr(ptr %print.tmp1)
  %multmp = call i64 @ardent_rt_mul_i64(i64 4, i64 5)
  %8 = insertvalue %ArdentValue { i32 0, i64 undef, i8 undef, ptr undef, i32 undef }, i64 %multmp, 1
  %9 = insertvalue %ArdentValue %8, i8 0, 2
  %10 = insertvalue %ArdentValue %9, ptr null, 3
  %11 = insertvalue %ArdentValue %10, i32 0, 4
  store %ArdentValue %11, ptr %print.tmp2, align 8
  call void @ardent_rt_print_av_ptr(ptr %print.tmp2)
  %divtmp = call i64 @ardent_rt_div_i64(i64 20, i64 5)
  %12 = insertvalue %ArdentValue { i32 0, i64 undef, i8 undef, ptr undef, i32 undef }, i64 %divtmp, 1
  %13 = insertvalue %ArdentValue %12, i8 0, 2
  %14 = insertvalue %ArdentValue %13, ptr null, 3
  %15 = insertvalue %ArdentValue %14, i32 0, 4
  store %ArdentValue %15, ptr %print.tmp3, align 8
  call void @ardent_rt_print_av_ptr(ptr %print.tmp3)
  ret i32 0
}

declare i64 @ardent_rt_add_i64(i64, i64)

declare void @ardent_rt_concat_av_ptr(ptr, ptr, ptr)

declare void @ardent_rt_print_av_ptr(ptr)

declare i64 @ardent_rt_sub_i64(i64, i64)

declare i64 @ardent_rt_mul_i64(i64, i64)

declare i64 @ardent_rt_div_i64(i64, i64)
