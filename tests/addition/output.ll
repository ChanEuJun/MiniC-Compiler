; ModuleID = 'mini-c'
source_filename = "mini-c"

declare i32 @print_int(i32)

define i32 @addition(i32 %n, i32 %m) {
entry:
  %result = alloca i32, align 4
  %n1 = alloca i32, align 4
  store i32 %n, ptr %n1, align 4
  %m2 = alloca i32, align 4
  store i32 %m, ptr %m2, align 4
  call void @llvm.memset.p0.i64(ptr %result, i8 0, i64 4, i1 false)
  %n3 = load i32, ptr %n1, align 4
  %m4 = load i32, ptr %m2, align 4
  %addtmp = add i32 %n3, %m4
  store i32 %addtmp, ptr %result, align 4
  %n5 = load i32, ptr %n1, align 4
  %0 = icmp eq i32 %n5, 4
  %1 = zext i1 %0 to i32
  %2 = icmp ne i32 %1, 0
  br i1 %2, label %then, label %else

then:                                             ; preds = %entry
  %n6 = load i32, ptr %n1, align 4
  %m7 = load i32, ptr %m2, align 4
  %addtmp8 = add i32 %n6, %m7
  %calltmp = call i32 @print_int(i32 %addtmp8)
  br label %ifcont

else:                                             ; preds = %entry
  %n9 = load i32, ptr %n1, align 4
  %m10 = load i32, ptr %m2, align 4
  %multmp = mul i32 %n9, %m10
  %calltmp11 = call i32 @print_int(i32 %multmp)
  br label %ifcont

ifcont:                                           ; preds = %else, %then
  %result12 = load i32, ptr %result, align 4
  ret i32 %result12
}

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: write)
declare void @llvm.memset.p0.i64(ptr writeonly captures(none), i8, i64, i1 immarg) #0

attributes #0 = { nocallback nofree nounwind willreturn memory(argmem: write) }
