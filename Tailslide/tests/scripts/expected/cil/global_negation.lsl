.assembly extern mscorlib {.ver 1:0:5000:0}
.assembly extern LslLibrary {.ver 0:1:0:0}
.assembly extern LslUserScript {.ver 0:1:0:0}
.assembly extern ScriptTypes {.ver 0:1:0:0}
.assembly 'LSL_00000000_0000_0000_0000_000000000000' {.ver 0:0:0:0}
.class public auto ansi serializable beforefieldinit LSL_00000000_0000_0000_0000_000000000000 extends class [LslUserScript]LindenLab.SecondLife.LslUserScript
{
.field public int32 'gi'
.field public int32 'gbig'
.field public float32 'gf'
.field public int32 'gs'
.method public hidebysig specialname rtspecialname instance default void .ctor () cil managed
{
.maxstack 500
ldarg.0
ldc.i4 -1
stfld int32 LSL_00000000_0000_0000_0000_000000000000::'gi'
ldarg.0
ldc.i4 -2147483648
stfld int32 LSL_00000000_0000_0000_0000_000000000000::'gbig'
ldarg.0
ldc.r8 (00 00 00 00 00 00 f0 bf)
stfld float32 LSL_00000000_0000_0000_0000_000000000000::'gf'
ldarg.0
ldc.i4 1
stfld int32 LSL_00000000_0000_0000_0000_000000000000::'gs'
ldarg.0
call instance void class [LslUserScript]LindenLab.SecondLife.LslUserScript::.ctor()
ret
}
.method public hidebysig instance default void edefaultstate_entry() cil managed
{
.maxstack 500
.locals init (int32, float32)
ldc.i4 1
neg
stloc.s 0
ldc.r8 (00 00 00 00 00 00 f0 3f)
neg
stloc.s 1
ret
}
}
