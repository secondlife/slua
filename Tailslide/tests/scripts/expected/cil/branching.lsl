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
ldc.i4 0
brfalse LabelTempJump0
ldc.i4 0
brfalse LabelTempJump2
ldc.r8 (00 00 00 00 00 00 f0 3f)
pop
LabelTempJump2:
br LabelTempJump1
LabelTempJump0:
ldc.r8 (00 00 00 00 00 00 f0 3f)
pop
LabelTempJump1:
ldc.i4 2
pop
ldc.i4 2
pop
LabelTempJump3:
ldc.i4 2
brfalse LabelTempJump4
ldc.r8 (00 00 00 00 00 00 00 40)
pop
ldc.i4 2
pop
br LabelTempJump3
LabelTempJump4:
LabelTempJump5:
ldc.i4 3
brfalse LabelTempJump6
ldc.r8 (00 00 00 00 00 00 08 40)
pop
br LabelTempJump5
LabelTempJump6:
LabelTempJump7:
ldc.r8 (00 00 00 00 00 00 10 40)
pop
ldc.i4 4
brtrue LabelTempJump7
ret
}
}
