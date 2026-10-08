.assembly extern mscorlib {.ver 1:0:5000:0}
.assembly extern LslLibrary {.ver 0:1:0:0}
.assembly extern LslUserScript {.ver 0:1:0:0}
.assembly extern ScriptTypes {.ver 0:1:0:0}
.assembly 'LSL_00000000_0000_0000_0000_000000000000' {.ver 0:0:0:0}
.class public auto ansi serializable beforefieldinit LSL_00000000_0000_0000_0000_000000000000 extends class [LslUserScript]LindenLab.SecondLife.LslUserScript
{
.method public hidebysig specialname rtspecialname instance default void .ctor () cil managed
{
.maxstack 500
ldarg.0
call instance void class [LslUserScript]LindenLab.SecondLife.LslUserScript::.ctor()
ret
}
.method public hidebysig instance default void edefaultstate_entry() cil managed
{
.maxstack 500
.locals init (int32, int32)
ldc.i4 1
stloc.s 0
ldc.i4 2
stloc.s 1
ldc.i4 3
stloc.s 0
ldc.i4 4
dup
stloc.s 0
stloc.s 1
ldc.i4 5
stloc.s 0
ldc.i4 6
stloc.s 0
ldc.i4 0
stloc.s 0
LabelTempJump0:
ldc.i4 3
ldloc.s 0
cgt
brfalse LabelTempJump1
ldc.i4 1
ldloc.s 0
add
stloc.s 0
br LabelTempJump0
LabelTempJump1:
ldc.r8 (00 00 00 00 00 00 f0 3f)
call float32 class [LslLibrary]LindenLab.SecondLife.Library::'llFrand'(float32)
pop
ldc.r8 (00 00 00 00 00 00 f0 3f)
call float32 class [LslLibrary]LindenLab.SecondLife.Library::'llFrand'(float32)
pop
ret
}
}
