; ModuleID = 'ardent_module'
source_filename = "ardent_module"

%ArdentValue = type { i32, i64, i8, ptr, i32 }

define i32 @ardent_entry() {
entry:
  %print.tmp2 = alloca %ArdentValue, align 8
  %print.tmp1 = alloca %ArdentValue, align 8
  %print.tmp = alloca %ArdentValue, align 8
  store %ArdentValue { i32 0, i64 1, i8 0, ptr null, i32 0 }, ptr %print.tmp, align 8
  call void @ardent_rt_print_av_ptr(ptr %print.tmp)
  store %ArdentValue { i32 0, i64 2, i8 0, ptr null, i32 0 }, ptr %print.tmp1, align 8
  call void @ardent_rt_print_av_ptr(ptr %print.tmp1)
  store %ArdentValue { i32 0, i64 3, i8 0, ptr null, i32 0 }, ptr %print.tmp2, align 8
  call void @ardent_rt_print_av_ptr(ptr %print.tmp2)
  ret i32 0
}

declare void @ardent_rt_print_av_ptr(ptr)
