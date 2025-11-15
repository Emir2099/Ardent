; ModuleID = 'ardent_module'
source_filename = "ardent_module"

%ArdentValue = type { i32, i64, i1, ptr, i32 }

@.ardent.str.0 = private unnamed_addr constant [10 x i8] c"Blessing \00", align 1
@.ardent.str.1 = private unnamed_addr constant [9 x i8] c"Blessed \00", align 1
@.ardent.str.2 = private unnamed_addr constant [31 x i8] c"--- Blessing & Return Demo ---\00", align 1
@.ardent.str.3 = private unnamed_addr constant [8 x i8] c"Boromir\00", align 1
@.ardent.str.4 = private unnamed_addr constant [6 x i8] c"Gimli\00", align 1

define %ArdentValue @spell_bless(%ArdentValue %name) {
entry:
  %print.tmp = alloca %ArdentValue, align 8
  %name1 = alloca %ArdentValue, align 8
  store %ArdentValue %name, ptr %name1, align 8
  %name2 = load %ArdentValue, ptr %name1, align 8
  %num = extractvalue %ArdentValue %name2, 1
  %addtmp = call i64 @ardent_rt_add_i64(i64 0, i64 %num)
  %0 = insertvalue %ArdentValue { i32 0, i64 undef, i1 undef, ptr undef, i32 undef }, i64 %addtmp, 1
  %1 = insertvalue %ArdentValue %0, i1 false, 2
  %2 = insertvalue %ArdentValue %1, ptr null, 3
  %3 = insertvalue %ArdentValue %2, i32 0, 4
  store %ArdentValue %3, ptr %print.tmp, align 8
  call void @ardent_rt_print_av_ptr(ptr %print.tmp)
  %name3 = load %ArdentValue, ptr %name1, align 8
  %num4 = extractvalue %ArdentValue %name3, 1
  %addtmp5 = call i64 @ardent_rt_add_i64(i64 0, i64 %num4)
  %4 = insertvalue %ArdentValue { i32 0, i64 undef, i1 undef, ptr undef, i32 undef }, i64 %addtmp5, 1
  %5 = insertvalue %ArdentValue %4, i1 false, 2
  %6 = insertvalue %ArdentValue %5, ptr null, 3
  %7 = insertvalue %ArdentValue %6, i32 0, 4
  ret %ArdentValue %7
}

declare i64 @ardent_rt_add_i64(i64, i64)

declare void @ardent_rt_print_av_ptr(ptr)

define i32 @ardent_entry() {
entry:
  %print.tmp4 = alloca %ArdentValue, align 8
  %result = alloca %ArdentValue, align 8
  %print.tmp1 = alloca %ArdentValue, align 8
  %print.tmp = alloca %ArdentValue, align 8
  store %ArdentValue { i32 1, i64 0, i1 false, ptr @.ardent.str.2, i32 30 }, ptr %print.tmp, align 8
  call void @ardent_rt_print_av_ptr(ptr %print.tmp)
  %bless.ret = call %ArdentValue @spell_bless(%ArdentValue { i32 1, i64 0, i1 false, ptr @.ardent.str.3, i32 7 })
  store %ArdentValue %bless.ret, ptr %print.tmp1, align 8
  call void @ardent_rt_print_av_ptr(ptr %print.tmp1)
  %bless.ret2 = call %ArdentValue @spell_bless(%ArdentValue { i32 1, i64 0, i1 false, ptr @.ardent.str.4, i32 5 })
  store %ArdentValue %bless.ret2, ptr %result, align 8
  %result3 = load %ArdentValue, ptr %result, align 8
  store %ArdentValue %result3, ptr %print.tmp4, align 8
  call void @ardent_rt_print_av_ptr(ptr %print.tmp4)
  ret i32 0
}
