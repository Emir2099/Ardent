; ModuleID = 'ardent_module'
source_filename = "ardent_module"

%ArdentValue = type { i32, i64, i1, ptr, i32 }

@.ardent.str.0 = private unnamed_addr constant [10 x i8] c"Vars Demo\00", align 1

define %ArdentValue @spell_setnum(%ArdentValue %n) {
entry:
  %print.tmp = alloca %ArdentValue, align 8
  %n1 = alloca %ArdentValue, align 8
  store %ArdentValue %n, ptr %n1, align 8
  %n2 = load %ArdentValue, ptr %n1, align 8
  store %ArdentValue %n2, ptr %print.tmp, align 8
  call void @ardent_rt_print_av_ptr(ptr %print.tmp)
  %n3 = load %ArdentValue, ptr %n1, align 8
  ret %ArdentValue %n3
}

declare void @ardent_rt_print_av_ptr(ptr)

define i32 @ardent_entry() {
entry:
  %print.tmp3 = alloca %ArdentValue, align 8
  %print.tmp1 = alloca %ArdentValue, align 8
  %print.tmp = alloca %ArdentValue, align 8
  store %ArdentValue { i32 1, i64 0, i1 false, ptr @.ardent.str.0, i32 9 }, ptr %print.tmp, align 8
  call void @ardent_rt_print_av_ptr(ptr %print.tmp)
  %setnum.ret = call %ArdentValue @spell_setnum(%ArdentValue { i32 0, i64 42, i1 false, ptr null, i32 0 })
  store %ArdentValue %setnum.ret, ptr %print.tmp1, align 8
  call void @ardent_rt_print_av_ptr(ptr %print.tmp1)
  %setnum.ret2 = call %ArdentValue @spell_setnum(%ArdentValue { i32 0, i64 7, i1 false, ptr null, i32 0 })
  store %ArdentValue %setnum.ret2, ptr %print.tmp3, align 8
  call void @ardent_rt_print_av_ptr(ptr %print.tmp3)
  ret i32 0
}
