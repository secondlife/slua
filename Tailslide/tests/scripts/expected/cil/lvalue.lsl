.assembly extern mscorlib {.ver 1:0:5000:0}
.assembly extern LslLibrary {.ver 0:1:0:0}
.assembly extern LslUserScript {.ver 0:1:0:0}
.assembly extern ScriptTypes {.ver 0:1:0:0}
.assembly 'LSL_00000000_0000_0000_0000_000000000000' {.ver 0:0:0:0}
.class public auto ansi serializable beforefieldinit LSL_00000000_0000_0000_0000_000000000000 extends class [LslUserScript]LindenLab.SecondLife.LslUserScript
{
.field public int32 'gi'
.field public class [ScriptTypes]LindenLab.SecondLife.Vector 'gv'
.field public class [ScriptTypes]LindenLab.SecondLife.Quaternion 'gr'
.method public hidebysig specialname rtspecialname instance default void .ctor () cil managed
{
.maxstack 500
ldarg.0
ldc.i4.0
stfld int32 LSL_00000000_0000_0000_0000_000000000000::'gi'
ldarg.0
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
stfld class [ScriptTypes]LindenLab.SecondLife.Vector LSL_00000000_0000_0000_0000_000000000000::'gv'
ldarg.0
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 f0 3f)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
stfld class [ScriptTypes]LindenLab.SecondLife.Quaternion LSL_00000000_0000_0000_0000_000000000000::'gr'
ldarg.0
call instance void class [LslUserScript]LindenLab.SecondLife.LslUserScript::.ctor()
ret
}
.method public hidebysig instance default void edefaultstate_entry() cil managed
{
.maxstack 500
.locals init (int32, class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Quaternion)
ldc.i4.0
stloc.s 0
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
stloc.s 1
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 f0 3f)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
stloc.s 2
ldloc.s 0
pop
ldarg.0
ldfld int32 LSL_00000000_0000_0000_0000_000000000000::'gi'
pop
ldloc.s 1
pop
ldarg.0
ldfld class [ScriptTypes]LindenLab.SecondLife.Vector LSL_00000000_0000_0000_0000_000000000000::'gv'
pop
ldloca.s 1
ldfld float32 class [ScriptTypes]LindenLab.SecondLife.Vector::x
pop
ldarg.0
ldflda class [ScriptTypes]LindenLab.SecondLife.Vector LSL_00000000_0000_0000_0000_000000000000::'gv'
ldfld float32 class [ScriptTypes]LindenLab.SecondLife.Vector::x
pop
ldloca.s 2
ldfld float32 class [ScriptTypes]LindenLab.SecondLife.Quaternion::s
pop
ldarg.0
ldflda class [ScriptTypes]LindenLab.SecondLife.Quaternion LSL_00000000_0000_0000_0000_000000000000::'gr'
ldfld float32 class [ScriptTypes]LindenLab.SecondLife.Quaternion::s
pop
ldstr "00000000-0000-0000-0000-000000000000"
pop
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
pop
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 f0 3f)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
pop
ret
}
}
