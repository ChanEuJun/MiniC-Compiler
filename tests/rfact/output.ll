; ModuleID = 'mini-c'
source_filename = "mini-c"

define i32 @multiplyNumbers(i32 %n) {
entry:
  %result = alloca i32, align 4
  %n1 = alloca i32, align 4
  store i32 %n, ptr %n1, align 4
  call void @llvm.memset.p0.i64(ptr %result, i8 0, i64 4, i1 false)
  store i32 0, ptr %result, align 4
  %n2 = load i32, ptr %n1, align 4
  %0 = icmp sge i32 %n2, 1
  %1 = zext i1 %0 to i32
  %2 = icmp ne i32 %1, 0
  br i1 %2, label %then, label %else

then:                                             ; preds = %entry
  %n3 = load i32, ptr %n1, align 4
  %n4 = load i32, ptr %n1, align 4
  %subtmp = sub i32 %n4, 1
  %calltmp = call i32 @multiplyNumbers(i32 %subtmp)
  %multmp = mul i32 %n3, %calltmp
  store i32 %multmp, ptr %result, align 4
  br label %ifcont

else:                                             ; preds = %entry
  store i32 1, ptr %result, align 4
  br label %ifcont

ifcont:                                           ; preds = %else, %then
  %result5 = load i32, ptr %result, align 4
  ret i32 %result5
}

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: write)
declare void @llvm.memset.p0.i64(ptr writeonly captures(none), i8, i64, i1 immarg) #0

define i32 @rfact(i32 %n) {
entry:
  %n1 = alloca i32, align 4
  store i32 %n, ptr %n1, align 4
  %n2 = load i32, ptr %n1, align 4
  %calltmp = call i32 @multiplyNumbers(i32 %n2)
  ret i32 %calltmp
}

attributes #0 = { nocallback nofree nounwind willreturn memory(argmem: write) }
