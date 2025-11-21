; ModuleID = 'mini-c'
source_filename = "mini-c"

declare i32 @print_int(i32)

define i32 @addNumbers(i32 %n) {
entry:
  %result = alloca i32, align 4
  %n1 = alloca i32, align 4
  store i32 %n, ptr %n1, align 4
  call void @llvm.memset.p0.i64(ptr %result, i8 0, i64 4, i1 false)
  store i32 0, ptr %result, align 4
  %n2 = load i32, ptr %n1, align 4
  %0 = icmp ne i32 %n2, 0
  %1 = zext i1 %0 to i32
  %2 = icmp ne i32 %1, 0
  br i1 %2, label %then, label %else

then:                                             ; preds = %entry
  %n3 = load i32, ptr %n1, align 4
  %n4 = load i32, ptr %n1, align 4
  %subtmp = sub i32 %n4, 1
  %calltmp = call i32 @addNumbers(i32 %subtmp)
  %addtmp = add i32 %n3, %calltmp
  store i32 %addtmp, ptr %result, align 4
  br label %ifcont

else:                                             ; preds = %entry
  %n5 = load i32, ptr %n1, align 4
  store i32 %n5, ptr %result, align 4
  br label %ifcont

ifcont:                                           ; preds = %else, %then
  %result6 = load i32, ptr %result, align 4
  %calltmp7 = call i32 @print_int(i32 %result6)
  %result8 = load i32, ptr %result, align 4
  ret i32 %result8
}

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: write)
declare void @llvm.memset.p0.i64(ptr writeonly captures(none), i8, i64, i1 immarg) #0

define i32 @recursion_driver(i32 %num) {
entry:
  %num1 = alloca i32, align 4
  store i32 %num, ptr %num1, align 4
  %num2 = load i32, ptr %num1, align 4
  %calltmp = call i32 @addNumbers(i32 %num2)
  ret i32 %calltmp
}

attributes #0 = { nocallback nofree nounwind willreturn memory(argmem: write) }
