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
.locals init (class [ScriptTypes]LindenLab.SecondLife.Vector)
ldc.i4 1
conv.r8
ldc.i4 2
conv.r8
ldc.i4 3
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
stloc.s 0
ldloca.s 0
ldc.i4 2
conv.r8
stfld float32 class [ScriptTypes]LindenLab.SecondLife.Vector::x
ldloca.s 0
ldfld float32 class [ScriptTypes]LindenLab.SecondLife.Vector::x
pop
ldc.i4 1
conv.r8
ldc.i4 2
conv.r8
ldc.i4 3
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
ldloca.s 0
ldfld float32 class [ScriptTypes]LindenLab.SecondLife.Vector::x
ldc.i4 1
conv.r8
ldc.i4 2
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call float32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Multiply'(class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
ldc.i4 2
conv.r8
ldc.i4 1
conv.r8
ldc.i4 2
conv.r8
ldc.i4 3
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
ldloca.s 0
ldfld float32 class [ScriptTypes]LindenLab.SecondLife.Vector::x
ldc.i4 1
conv.r8
ldc.i4 2
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call float32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Multiply'(class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
dup
stloc.s 0
pop
ldloc.s 0
call void class [LslLibrary]LindenLab.SecondLife.Library::'llSetPos'(class [ScriptTypes]LindenLab.SecondLife.Vector)
ret
}
}
