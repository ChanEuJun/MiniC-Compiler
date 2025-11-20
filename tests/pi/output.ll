; ModuleID = 'mini-c'
source_filename = "mini-c"

define float @pi() {
entry:
  %i = alloca i32, align 4
  %PI = alloca float, align 4
  %flag = alloca i1, align 1
  store i1 false, ptr %flag, align 1
  store float 0.000000e+00, ptr %PI, align 4
  store i32 0, ptr %i, align 4
  store i1 true, ptr %flag, align 1
  store double 3.000000e+00, ptr %PI, align 8
  store i32 2, ptr %i, align 4
  br label %loop

loop:                                             ; preds = %ifcont, %entry
  %i1 = load i32, ptr %i, align 4
  %cmptmp = icmp slt i32 %i1, 100
  br i1 %cmptmp, label %loopbody, label %afterloop

loopbody:                                         ; preds = %loop
  %flag2 = load i1, ptr %flag, align 1
  br i1 %flag2, label %then, label %else

then:                                             ; preds = %loopbody
  %PI3 = load float, ptr %PI, align 4
  %i4 = load i32, ptr %i, align 4
  %i5 = load i32, ptr %i, align 4
  %addtmp = add i32 %i5, 1
  %multmp = mul i32 %i4, %addtmp
  %i6 = load i32, ptr %i, align 4
  %addtmp7 = add i32 %i6, 2
  %multmp8 = mul i32 %multmp, %addtmp7
  %divtmp = sdiv double 4.000000e+00, i32 %multmp8
  %addtmp9 = fadd float %PI3, double %divtmp
  store float %addtmp9, ptr %PI, align 4
  br label %ifcont

else:                                             ; preds = %loopbody
  %PI10 = load float, ptr %PI, align 4
  %i11 = load i32, ptr %i, align 4
  %i12 = load i32, ptr %i, align 4
  %addtmp13 = add i32 %i12, 1
  %multmp14 = mul i32 %i11, %addtmp13
  %i15 = load i32, ptr %i, align 4
  %addtmp16 = add i32 %i15, 2
  %multmp17 = mul i32 %multmp14, %addtmp16
  %divtmp18 = sdiv double 4.000000e+00, i32 %multmp17
  %subtmp = fsub float %PI10, double %divtmp18
  store float %subtmp, ptr %PI, align 4
  br label %ifcont

ifcont:                                           ; preds = %else, %then
  %flag19 = load i1, ptr %flag, align 1
  %nottmp = xor i1 %flag19, true
  store i1 %nottmp, ptr %flag, align 1
  %i20 = load i32, ptr %i, align 4
  %addtmp21 = add i32 %i20, 2
  store i32 %addtmp21, ptr %i, align 4
  br label %loop

afterloop:                                        ; preds = %loop
  %PI22 = load float, ptr %PI, align 4
  ret float %PI22
}
