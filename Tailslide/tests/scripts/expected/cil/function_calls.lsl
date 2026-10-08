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
.method public hidebysig instance default int32 'gfoo'(int32 'i', int32 'g') cil managed
{
.maxstack 500
ldarg.s 'i'
ret
}
.method public hidebysig instance default void 'gbar'(int32 'i') cil managed
{
.maxstack 500
ldarg.s 'i'
pop
ret
}
.method public hidebysig instance default void edefaultstate_entry() cil managed
{
.maxstack 500
ldc.r8 (00 00 00 00 00 00 f0 3f)
call float32 class [LslLibrary]LindenLab.SecondLife.Library::'llFrand'(float32)
pop
ldarg.0
ldc.i4 1
ldc.i4 2
call instance int32 class LSL_00000000_0000_0000_0000_000000000000::'gfoo'(int32, int32)
pop
ret
}
}
