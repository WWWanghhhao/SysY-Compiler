declare i32 @getint()
declare i32 @getch()
declare float @getfloat()
declare i32 @getarray(ptr)
declare i32 @getfarray(ptr)
declare void @putint(i32)
declare void @putch(i32)
declare void @putfloat(float)
declare void @putarray(i32,ptr)
declare void @putfarray(i32,ptr)
declare void @_sysy_starttime(i32)
declare void @_sysy_stoptime(i32)
declare void @llvm.memset.p0.i32(ptr,i8,i32,i1)
declare i32 @llvm.umax.i32(i32,i32)
declare i32 @llvm.umin.i32(i32,i32)
declare i32 @llvm.smax.i32(i32,i32)
declare i32 @llvm.smin.i32(i32,i32)
define i32 @main()
{
L0:  ;
    %r0 = alloca i32
    br label %L1
L1:  ;
    %r3 = add i32 5,0
    store i32 %r3, ptr %r0
    %r4 = load i32, ptr %r0
    %r5 = add i32 4,0
    %r6 = icmp sgt i32 %r4,%r5
    br i1 %r6, label %L4, label %L2
L2:  ;
    %r10 = add i32 3,0
    store i32 %r10, ptr %r0
    br label %L3
L3:  ;
    %r12 = load i32, ptr %r0
    ret i32 %r12
L4:  ;
    %r8 = add i32 4,0
    store i32 %r8, ptr %r0
    br label %L3
}
