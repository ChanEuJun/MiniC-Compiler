; ModuleID = 'mini-c'
source_filename = "mini-c"

@test = common global i32 0
@f = common global float 0.000000e+00
@b = common global i1 false

declare i32 @print_int(i32)

define i32 @While(i32 %n) {
entry:
  %result = alloca i32, align 4
  %n1 = alloca i32, align 4
  store i32 %n, ptr %n1, align 4
  call void @llvm.memset.p0.i64(ptr %result, i8 0, i64 4, i1 false)
  store i32 12, ptr @test, align 4
  store i32 0, ptr %result, align 4
  %test = load i32, ptr @test, align 4
  %calltmp = call i32 @print_int(i32 %test)
  br label %loop

loop:                                             ; preds = %loopbody, %entry
  %result2 = load i32, ptr %result, align 4
  %0 = icmp slt i32 %result2, 10
  %1 = zext i1 %0 to i32
  %2 = icmp ne i32 %1, 0
  br i1 %2, label %loopbody, label %afterloop

loopbody:                                         ; preds = %loop
  %result3 = load i32, ptr %result, align 4
  %addtmp = add i32 %result3, 1
  store i32 %addtmp, ptr %result, align 4
  br label %loop

afterloop:                                        ; preds = %loop
  %result4 = load i32, ptr %result, align 4
  ret i32 %result4
}

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: write)
declare void @llvm.memset.p0.i64(ptr writeonly captures(none), i8, i64, i1 immarg) #0

attributes #0 = { nocallback nofree nounwind willreturn memory(argmem: write) }
