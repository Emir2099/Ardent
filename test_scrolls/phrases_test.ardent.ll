; ModuleID = 'ardent_module'
source_filename = "ardent_module"

%ArdentValue = type { i32, i64, i1, ptr, i32 }

@.ardent.str.0 = private unnamed_addr constant [6 x i8] c"Hello\00", align 1
@.ardent.str.1 = private unnamed_addr constant [6 x i8] c"World\00", align 1
@.ardent.str.2 = private unnamed_addr constant [6 x i8] c"Hello\00", align 1
@.ardent.str.3 = private unnamed_addr constant [2 x i8] c" \00", align 1
@.ardent.str.4 = private unnamed_addr constant [6 x i8] c"World\00", align 1

define i32 @ardent_entry() {
entry:
  %print.tmp10 = alloca %ArdentValue, align 8
  %concat.right8 = alloca %ArdentValue, align 8
  %concat.left7 = alloca %ArdentValue, align 8
  %plus.result5 = alloca %ArdentValue, align 8
  %concat.right = alloca %ArdentValue, align 8
  %concat.left = alloca %ArdentValue, align 8
  %plus.result = alloca %ArdentValue, align 8
  %print.tmp1 = alloca %ArdentValue, align 8
  %print.tmp = alloca %ArdentValue, align 8
  store %ArdentValue { i32 1, i64 0, i1 false, ptr @.ardent.str.0, i32 5 }, ptr %print.tmp, align 8
  call void @ardent_rt_print_av_ptr(ptr %print.tmp)
  store %ArdentValue { i32 1, i64 0, i1 false, ptr @.ardent.str.1, i32 5 }, ptr %print.tmp1, align 8
  call void @ardent_rt_print_av_ptr(ptr %print.tmp1)
  br i1 false, label %plus.num, label %plus.concat

plus.num:                                         ; preds = %entry
  %addtmp = call i64 @ardent_rt_add_i64(i64 0, i64 0)
  %0 = insertvalue %ArdentValue { i32 0, i64 undef, i1 undef, ptr undef, i32 undef }, i64 %addtmp, 1
  %1 = insertvalue %ArdentValue %0, i1 false, 2
  %2 = insertvalue %ArdentValue %1, ptr null, 3
  %3 = insertvalue %ArdentValue %2, i32 0, 4
  store %ArdentValue %3, ptr %plus.result, align 8
  br label %plus.merge

plus.concat:                                      ; preds = %entry
  store %ArdentValue { i32 1, i64 0, i1 false, ptr @.ardent.str.2, i32 5 }, ptr %concat.left, align 8
  store %ArdentValue { i32 1, i64 0, i1 false, ptr @.ardent.str.3, i32 1 }, ptr %concat.right, align 8
  call void @ardent_rt_concat_av_ptr(ptr %concat.left, ptr %concat.right, ptr %plus.result)
  br label %plus.merge

plus.merge:                                       ; preds = %plus.concat, %plus.num
  %plus.final = load %ArdentValue, ptr %plus.result, align 8
  %num = extractvalue %ArdentValue %plus.final, 1
  %tagL = extractvalue %ArdentValue %plus.final, 0
  %isNumL = icmp eq i32 %tagL, 0
  %bothNums = and i1 %isNumL, false
  br i1 %bothNums, label %plus.num2, label %plus.concat3

plus.num2:                                        ; preds = %plus.merge
  %addtmp6 = call i64 @ardent_rt_add_i64(i64 %num, i64 0)
  %4 = insertvalue %ArdentValue { i32 0, i64 undef, i1 undef, ptr undef, i32 undef }, i64 %addtmp6, 1
  %5 = insertvalue %ArdentValue %4, i1 false, 2
  %6 = insertvalue %ArdentValue %5, ptr null, 3
  %7 = insertvalue %ArdentValue %6, i32 0, 4
  store %ArdentValue %7, ptr %plus.result5, align 8
  br label %plus.merge4

plus.concat3:                                     ; preds = %plus.merge
  store %ArdentValue %plus.final, ptr %concat.left7, align 8
  store %ArdentValue { i32 1, i64 0, i1 false, ptr @.ardent.str.4, i32 5 }, ptr %concat.right8, align 8
  call void @ardent_rt_concat_av_ptr(ptr %concat.left7, ptr %concat.right8, ptr %plus.result5)
  br label %plus.merge4

plus.merge4:                                      ; preds = %plus.concat3, %plus.num2
  %plus.final9 = load %ArdentValue, ptr %plus.result5, align 8
  store %ArdentValue %plus.final9, ptr %print.tmp10, align 8
  call void @ardent_rt_print_av_ptr(ptr %print.tmp10)
}

declare void @ardent_rt_print_av_ptr(ptr)

declare i64 @ardent_rt_add_i64(i64, i64)

declare void @ardent_rt_concat_av_ptr(ptr, ptr, ptr)
