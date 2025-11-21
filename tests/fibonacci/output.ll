; ModuleID = 'mini-c'
source_filename = "mini-c"

declare i32 @print_int(i32)

define i32 @fibonacci(i32 %n) {
entry:
  %total = alloca i32, align 4
  %c = alloca i32, align 4
  %next = alloca i32, align 4
  %second = alloca i32, align 4
  %first = alloca i32, align 4
  %n1 = alloca i32, align 4
  store i32 %n, ptr %n1, align 4
  call void @llvm.memset.p0.i64(ptr %first, i8 0, i64 4, i1 false)
  call void @llvm.memset.p0.i64(ptr %second, i8 0, i64 4, i1 false)
  call void @llvm.memset.p0.i64(ptr %next, i8 0, i64 4, i1 false)
  call void @llvm.memset.p0.i64(ptr %c, i8 0, i64 4, i1 false)
  call void @llvm.memset.p0.i64(ptr %total, i8 0, i64 4, i1 false)
  %n2 = load i32, ptr %n1, align 4
  %calltmp = call i32 @print_int(i32 %n2)
  store i32 0, ptr %first, align 4
  store i32 1, ptr %second, align 4
  store i32 1, ptr %c, align 4
  store i32 0, ptr %total, align 4
  br label %loop

loop:                                             ; preds = %ifcont, %entry
  %c3 = load i32, ptr %c, align 4
  %n4 = load i32, ptr %n1, align 4
  %0 = icmp slt i32 %c3, %n4
  %1 = zext i1 %0 to i32
  %2 = icmp ne i32 %1, 0
  br i1 %2, label %loopbody, label %afterloop

loopbody:                                         ; preds = %loop
  %c5 = load i32, ptr %c, align 4
  %3 = icmp sle i32 %c5, 1
  %4 = zext i1 %3 to i32
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %then, label %else

then:                                             ; preds = %loopbody
  %c6 = load i32, ptr %c, align 4
  store i32 %c6, ptr %next, align 4
  br label %ifcont

else:                                             ; preds = %loopbody
  %first7 = load i32, ptr %first, align 4
  %second8 = load i32, ptr %second, align 4
  %addtmp = add i32 %first7, %second8
  store i32 %addtmp, ptr %next, align 4
  %second9 = load i32, ptr %second, align 4
  store i32 %second9, ptr %first, align 4
  %next10 = load i32, ptr %next, align 4
  store i32 %next10, ptr %second, align 4
  br label %ifcont

ifcont:                                           ; preds = %else, %then
  %next11 = load i32, ptr %next, align 4
  %calltmp12 = call i32 @print_int(i32 %next11)
  %c13 = load i32, ptr %c, align 4
  %addtmp14 = add i32 %c13, 1
  store i32 %addtmp14, ptr %c, align 4
  %total15 = load i32, ptr %total, align 4
  %next16 = load i32, ptr %next, align 4
  %addtmp17 = add i32 %total15, %next16
  store i32 %addtmp17, ptr %total, align 4
  br label %loop

afterloop:                                        ; preds = %loop
  %total18 = load i32, ptr %total, align 4
  %calltmp19 = call i32 @print_int(i32 %total18)
  %total20 = load i32, ptr %total, align 4
  ret i32 %total20
}

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: write)
declare void @llvm.memset.p0.i64(ptr writeonly captures(none), i8, i64, i1 immarg) #0

attributes #0 = { nocallback nofree nounwind willreturn memory(argmem: write) }
