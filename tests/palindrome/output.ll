; ModuleID = 'mini-c'
source_filename = "mini-c"

define i1 @palindrome(i32 %number) {
entry:
  %result = alloca i1, align 1
  %rmndr = alloca i32, align 4
  %rev = alloca i32, align 4
  %t = alloca i32, align 4
  %number1 = alloca i32, align 4
  store i32 %number, ptr %number1, align 4
  call void @llvm.memset.p0.i64(ptr %t, i8 0, i64 4, i1 false)
  call void @llvm.memset.p0.i64(ptr %rev, i8 0, i64 4, i1 false)
  call void @llvm.memset.p0.i64(ptr %rmndr, i8 0, i64 4, i1 false)
  call void @llvm.memset.p0.i64(ptr %result, i8 0, i64 1, i1 false)
  store i32 0, ptr %rev, align 4
  store i32 0, ptr %result, align 4
  %number2 = load i32, ptr %number1, align 4
  store i32 %number2, ptr %t, align 4
  br label %loop

loop:                                             ; preds = %loopbody, %entry
  %number3 = load i32, ptr %number1, align 4
  %0 = icmp sgt i32 %number3, 0
  %1 = zext i1 %0 to i32
  %2 = icmp ne i32 %1, 0
  br i1 %2, label %loopbody, label %afterloop

loopbody:                                         ; preds = %loop
  %number4 = load i32, ptr %number1, align 4
  %remtmp = srem i32 %number4, 10
  store i32 %remtmp, ptr %rmndr, align 4
  %rev5 = load i32, ptr %rev, align 4
  %multmp = mul i32 %rev5, 10
  %rmndr6 = load i32, ptr %rmndr, align 4
  %addtmp = add i32 %multmp, %rmndr6
  store i32 %addtmp, ptr %rev, align 4
  %number7 = load i32, ptr %number1, align 4
  %divtmp = sdiv i32 %number7, 10
  store i32 %divtmp, ptr %number1, align 4
  br label %loop

afterloop:                                        ; preds = %loop
  %t8 = load i32, ptr %t, align 4
  %rev9 = load i32, ptr %rev, align 4
  %3 = icmp eq i32 %t8, %rev9
  %4 = zext i1 %3 to i32
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %then, label %else

then:                                             ; preds = %afterloop
  store i32 1, ptr %result, align 4
  br label %ifcont

else:                                             ; preds = %afterloop
  store i32 0, ptr %result, align 4
  br label %ifcont

ifcont:                                           ; preds = %else, %then
  %result10 = load i1, ptr %result, align 1
  ret i1 %result10
}

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: write)
declare void @llvm.memset.p0.i64(ptr writeonly captures(none), i8, i64, i1 immarg) #0

attributes #0 = { nocallback nofree nounwind willreturn memory(argmem: write) }
