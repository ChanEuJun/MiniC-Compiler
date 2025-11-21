; ModuleID = 'mini-c'
source_filename = "mini-c"

define i32 @factorial(i32 %n) {
entry:
  %factorial = alloca i32, align 4
  %i = alloca i32, align 4
  %n1 = alloca i32, align 4
  store i32 %n, ptr %n1, align 4
  call void @llvm.memset.p0.i64(ptr %i, i8 0, i64 4, i1 false)
  call void @llvm.memset.p0.i64(ptr %factorial, i8 0, i64 4, i1 false)
  store i32 1, ptr %factorial, align 4
  store i32 1, ptr %i, align 4
  br label %loop

loop:                                             ; preds = %loopbody, %entry
  %i2 = load i32, ptr %i, align 4
  %n3 = load i32, ptr %n1, align 4
  %0 = icmp sle i32 %i2, %n3
  %1 = zext i1 %0 to i32
  %2 = icmp ne i32 %1, 0
  br i1 %2, label %loopbody, label %afterloop

loopbody:                                         ; preds = %loop
  %factorial4 = load i32, ptr %factorial, align 4
  %i5 = load i32, ptr %i, align 4
  %multmp = mul i32 %factorial4, %i5
  store i32 %multmp, ptr %factorial, align 4
  %i6 = load i32, ptr %i, align 4
  %addtmp = add i32 %i6, 1
  store i32 %addtmp, ptr %i, align 4
  br label %loop

afterloop:                                        ; preds = %loop
  %factorial7 = load i32, ptr %factorial, align 4
  ret i32 %factorial7
}

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: write)
declare void @llvm.memset.p0.i64(ptr writeonly captures(none), i8, i64, i1 immarg) #0

attributes #0 = { nocallback nofree nounwind willreturn memory(argmem: write) }
