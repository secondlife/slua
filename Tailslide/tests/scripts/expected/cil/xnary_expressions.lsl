.assembly extern mscorlib {.ver 1:0:5000:0}
.assembly extern LslLibrary {.ver 0:1:0:0}
.assembly extern LslUserScript {.ver 0:1:0:0}
.assembly extern ScriptTypes {.ver 0:1:0:0}
.assembly 'LSL_00000000_0000_0000_0000_000000000000' {.ver 0:0:0:0}
.class public auto ansi serializable beforefieldinit LSL_00000000_0000_0000_0000_000000000000 extends class [LslUserScript]LindenLab.SecondLife.LslUserScript
{
.field public int32 'gi'
.field public float32 'gf'
.method public hidebysig specialname rtspecialname instance default void .ctor () cil managed
{
.maxstack 500
ldarg.0
ldc.i4.0
stfld int32 LSL_00000000_0000_0000_0000_000000000000::'gi'
ldarg.0
ldc.r8 (00 00 00 00 00 00 00 00)
stfld float32 LSL_00000000_0000_0000_0000_000000000000::'gf'
ldarg.0
call instance void class [LslUserScript]LindenLab.SecondLife.LslUserScript::.ctor()
ret
}
.method public hidebysig instance default void edefaultstate_entry() cil managed
{
.maxstack 500
.locals init (int32, float32)
ldc.i4.0
stloc.s 0
ldc.r8 (00 00 00 00 00 00 00 00)
stloc.s 1
ldc.i4 1
dup
stloc.s 0
pop
ldc.r8 (00 00 00 00 00 00 f0 3f)
dup
stloc.s 1
pop
ldarg.0
ldc.i4 1
stfld int32 LSL_00000000_0000_0000_0000_000000000000::'gi'
ldarg.0
ldfld int32 LSL_00000000_0000_0000_0000_000000000000::'gi'
pop
ldarg.0
ldc.r8 (00 00 00 00 00 00 f0 3f)
stfld float32 LSL_00000000_0000_0000_0000_000000000000::'gf'
ldarg.0
ldfld float32 LSL_00000000_0000_0000_0000_000000000000::'gf'
pop
ldc.i4 1
ldc.i4 1
add
pop
ldc.r8 (00 00 00 00 00 00 00 40)
ldc.i4 2
conv.r8
call float64 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Subtract'(float64, float64)
pop
ldc.r8 (00 00 00 00 00 00 00 40)
ldc.i4 1
conv.r8
call float64 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Divide'(float64, float64)
pop
ldc.i4 2
ldc.i4 1
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Modulo'(int32, int32)
pop
ldc.r8 (00 00 00 00 00 00 00 40)
ldc.i4 1
conv.r8
cgt
ldc.i4.0
ceq
pop
ldc.r8 (00 00 00 00 00 00 00 40)
ldc.i4 1
conv.r8
clt
ldc.i4.0
ceq
pop
ldc.r8 (00 00 00 00 00 00 00 40)
ldc.i4 1
conv.r8
clt
pop
ldc.r8 (00 00 00 00 00 00 00 40)
ldc.i4 1
conv.r8
cgt
pop
ldc.r8 (00 00 00 00 00 00 00 40)
ldc.i4 1
conv.r8
ceq
pop
ldc.r8 (00 00 00 00 00 00 00 40)
ldc.i4 1
conv.r8
ceq
ldc.i4.0
ceq
pop
ldc.i4 2
ldc.i4.0
ceq
ldc.i4 1
ldc.i4.0
ceq
or
ldc.i4.0
ceq
pop
ldc.i4 2
ldc.i4 1
or
ldc.i4.0
ceq
ldc.i4.0
ceq
pop
ldc.i4 2
ldc.i4 1
or
pop
ldc.i4 2
ldc.i4 1
and
pop
ldc.i4 2
ldc.i4 1
xor
pop
ldc.i4 2
ldc.i4 1
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ShiftRight'(int32, int32)
pop
ldc.i4 2
ldc.i4 1
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ShiftLeft'(int32, int32)
pop
ldc.i4 2
ldloc.s 0
mul
dup
stloc.s 0
pop
ldarg.0
ldc.i4 2
ldarg.0
ldfld int32 LSL_00000000_0000_0000_0000_000000000000::'gi'
mul
stfld int32 LSL_00000000_0000_0000_0000_000000000000::'gi'
ldarg.0
ldfld int32 LSL_00000000_0000_0000_0000_000000000000::'gi'
pop
ldarg.0
ldc.i4 2
conv.r8
ldarg.0
ldfld float32 LSL_00000000_0000_0000_0000_000000000000::'gf'
mul
stfld float32 LSL_00000000_0000_0000_0000_000000000000::'gf'
ldarg.0
ldfld float32 LSL_00000000_0000_0000_0000_000000000000::'gf'
pop
ldarg.0
ldc.r8 (00 00 00 00 00 00 f0 3f)
ldarg.0
ldfld float32 LSL_00000000_0000_0000_0000_000000000000::'gf'
call float64 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Divide'(float64, float64)
stfld float32 LSL_00000000_0000_0000_0000_000000000000::'gf'
ldarg.0
ldfld float32 LSL_00000000_0000_0000_0000_000000000000::'gf'
pop
ldarg.0
ldc.i4 2
ldarg.0
ldfld int32 LSL_00000000_0000_0000_0000_000000000000::'gi'
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Modulo'(int32, int32)
stfld int32 LSL_00000000_0000_0000_0000_000000000000::'gi'
ldarg.0
ldfld int32 LSL_00000000_0000_0000_0000_000000000000::'gi'
pop
ldarg.0
ldc.r8 (00 00 00 00 00 00 f0 3f)
ldarg.0
ldfld float32 LSL_00000000_0000_0000_0000_000000000000::'gf'
add
stfld float32 LSL_00000000_0000_0000_0000_000000000000::'gf'
ldarg.0
ldfld float32 LSL_00000000_0000_0000_0000_000000000000::'gf'
pop
ldarg.0
ldc.r8 (00 00 00 00 00 00 f0 3f)
ldarg.0
ldfld float32 LSL_00000000_0000_0000_0000_000000000000::'gf'
call float64 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Subtract'(float64, float64)
stfld float32 LSL_00000000_0000_0000_0000_000000000000::'gf'
ldarg.0
ldfld float32 LSL_00000000_0000_0000_0000_000000000000::'gf'
pop
ldarg.0
ldc.r8 (00 00 00 00 00 00 f0 3f)
ldarg.0
ldfld int32 LSL_00000000_0000_0000_0000_000000000000::'gi'
conv.r8
mul
call int32 class [LslLibrary]LindenLab.SecondLife.LslRunTime::ToInteger(float32)
stfld int32 LSL_00000000_0000_0000_0000_000000000000::'gi'
ldarg.0
ldfld int32 LSL_00000000_0000_0000_0000_000000000000::'gi'
pop
ldarg.0
ldarg.0
ldc.r8 (00 00 00 00 00 00 f0 3f)
ldarg.0
ldfld int32 LSL_00000000_0000_0000_0000_000000000000::'gi'
conv.r8
mul
call int32 class [LslLibrary]LindenLab.SecondLife.LslRunTime::ToInteger(float32)
stfld int32 LSL_00000000_0000_0000_0000_000000000000::'gi'
ldarg.0
ldfld int32 LSL_00000000_0000_0000_0000_000000000000::'gi'
stfld float32 LSL_00000000_0000_0000_0000_000000000000::'gf'
ldarg.0
ldfld float32 LSL_00000000_0000_0000_0000_000000000000::'gf'
pop
ldarg.0
ldarg.0
ldfld int32 LSL_00000000_0000_0000_0000_000000000000::'gi'
ldc.i4.1
add
stfld int32 LSL_00000000_0000_0000_0000_000000000000::'gi'
ldarg.0
ldfld int32 LSL_00000000_0000_0000_0000_000000000000::'gi'
pop
ldarg.0
ldarg.0
ldfld int32 LSL_00000000_0000_0000_0000_000000000000::'gi'
ldc.i4.1
sub
stfld int32 LSL_00000000_0000_0000_0000_000000000000::'gi'
ldarg.0
ldfld int32 LSL_00000000_0000_0000_0000_000000000000::'gi'
pop
ldarg.0
ldfld int32 LSL_00000000_0000_0000_0000_000000000000::'gi'
ldc.i4.0
ceq
pop
ldarg.0
ldfld int32 LSL_00000000_0000_0000_0000_000000000000::'gi'
not
pop
ldarg.0
ldfld int32 LSL_00000000_0000_0000_0000_000000000000::'gi'
neg
pop
ldarg.0
ldfld int32 LSL_00000000_0000_0000_0000_000000000000::'gi'
ldarg.0
ldarg.0
ldfld int32 LSL_00000000_0000_0000_0000_000000000000::'gi'
ldc.i4.1
sub
stfld int32 LSL_00000000_0000_0000_0000_000000000000::'gi'
ldarg.0
ldfld int32 LSL_00000000_0000_0000_0000_000000000000::'gi'
pop
pop
ldarg.0
ldfld int32 LSL_00000000_0000_0000_0000_000000000000::'gi'
ldarg.0
ldarg.0
ldfld int32 LSL_00000000_0000_0000_0000_000000000000::'gi'
ldc.i4.1
add
stfld int32 LSL_00000000_0000_0000_0000_000000000000::'gi'
ldarg.0
ldfld int32 LSL_00000000_0000_0000_0000_000000000000::'gi'
pop
pop
ldc.i4 1
neg
pop
ldc.r8 (00 00 00 00 00 00 f0 3f)
neg
pop
ret
}
}
