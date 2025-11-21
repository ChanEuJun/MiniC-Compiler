; ModuleID = 'mini-c'
source_filename = "mini-c"

declare i32 @print_int(i32)

define i32 @is_leap(i32 %year) {
entry:
  %temp = alloca i32, align 4
  %isLeap = alloca i32, align 4
  %year1 = alloca i32, align 4
  store i32 %year, ptr %year1, align 4
  call void @llvm.memset.p0.i64(ptr %isLeap, i8 0, i64 4, i1 false)
  call void @llvm.memset.p0.i64(ptr %temp, i8 0, i64 4, i1 false)
  %year2 = load i32, ptr %year1, align 4
  %remtmp = srem i32 %year2, 4
  %0 = icmp eq i32 %remtmp, 0
  %1 = zext i1 %0 to i32
  %2 = icmp ne i32 %1, 0
  br i1 %2, label %then, label %else11

then:                                             ; preds = %entry
  %year3 = load i32, ptr %year1, align 4
  %remtmp4 = srem i32 %year3, 100
  %3 = icmp eq i32 %remtmp4, 0
  %4 = zext i1 %3 to i32
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %then5, label %else9

then5:                                            ; preds = %then
  %year6 = load i32, ptr %year1, align 4
  %remtmp7 = srem i32 %year6, 400
  %6 = icmp eq i32 %remtmp7, 0
  %7 = zext i1 %6 to i32
  %8 = icmp ne i32 %7, 0
  br i1 %8, label %then8, label %else

then8:                                            ; preds = %then5
  store i32 1, ptr %isLeap, align 4
  br label %ifcont

else:                                             ; preds = %then5
  store i32 0, ptr %isLeap, align 4
  br label %ifcont

ifcont:                                           ; preds = %else, %then8
  br label %ifcont10

else9:                                            ; preds = %then
  store i32 1, ptr %isLeap, align 4
  br label %ifcont10

ifcont10:                                         ; preds = %else9, %ifcont
  br label %ifcont12

else11:                                           ; preds = %entry
  store i32 0, ptr %isLeap, align 4
  br label %ifcont12

ifcont12:                                         ; preds = %else11, %ifcont10
  %isLeap13 = load i32, ptr %isLeap, align 4
  ret i32 %isLeap13
}

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: write)
declare void @llvm.memset.p0.i64(ptr writeonly captures(none), i8, i64, i1 immarg) #0

attributes #0 = { nocallback nofree nounwind willreturn memory(argmem: write) }
