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
.locals init (valuetype [ScriptTypes]LindenLab.SecondLife.Key, class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Quaternion)
ldstr ""
call valuetype [ScriptTypes]LindenLab.SecondLife.Key class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateKey'(string)
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
ldstr ""
ldstr ""
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Add'(string, string)
pop
ldloc.s 1
ldloc.s 1
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Add'(class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
pop
ldloc.s 2
ldloc.s 2
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Add'(class [ScriptTypes]LindenLab.SecondLife.Quaternion, class [ScriptTypes]LindenLab.SecondLife.Quaternion)
pop
ldc.i4 1
ldc.i4 1
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Subtract'(int32, int32)
pop
ldloc.s 1
ldloc.s 1
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Subtract'(class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
pop
ldloc.s 2
ldloc.s 2
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Subtract'(class [ScriptTypes]LindenLab.SecondLife.Quaternion, class [ScriptTypes]LindenLab.SecondLife.Quaternion)
pop
ldc.r8 (00 00 00 00 00 00 f0 3f)
ldloc.s 1
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Multiply'(float32, class [ScriptTypes]LindenLab.SecondLife.Vector)
pop
ldloc.s 1
ldc.r8 (00 00 00 00 00 00 f0 3f)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Multiply'(class [ScriptTypes]LindenLab.SecondLife.Vector, float32)
pop
ldloc.s 1
ldloc.s 1
call float32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Multiply'(class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
pop
ldloc.s 2
ldloc.s 1
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Multiply'(class [ScriptTypes]LindenLab.SecondLife.Quaternion, class [ScriptTypes]LindenLab.SecondLife.Vector)
pop
ldloc.s 2
ldloc.s 2
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Multiply'(class [ScriptTypes]LindenLab.SecondLife.Quaternion, class [ScriptTypes]LindenLab.SecondLife.Quaternion)
pop
ldc.i4 1
ldc.i4 1
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Divide'(int32, int32)
pop
ldc.r8 (00 00 00 00 00 00 f0 3f)
ldloc.s 1
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Divide'(float32, class [ScriptTypes]LindenLab.SecondLife.Vector)
pop
ldloc.s 2
ldloc.s 1
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Divide'(class [ScriptTypes]LindenLab.SecondLife.Quaternion, class [ScriptTypes]LindenLab.SecondLife.Vector)
pop
ldloc.s 2
ldloc.s 2
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Divide'(class [ScriptTypes]LindenLab.SecondLife.Quaternion, class [ScriptTypes]LindenLab.SecondLife.Quaternion)
pop
ldc.i4 1
ldc.i4 1
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Modulo'(int32, int32)
pop
ldloc.s 1
ldloc.s 1
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Modulo'(class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
pop
ldloc.s 0
ldloc.s 0
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Equals'(valuetype [ScriptTypes]LindenLab.SecondLife.Key, valuetype [ScriptTypes]LindenLab.SecondLife.Key)
pop
ldloc.s 1
ldloc.s 1
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Equals'(class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
pop
ldloc.s 2
ldloc.s 2
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Equals'(class [ScriptTypes]LindenLab.SecondLife.Quaternion, class [ScriptTypes]LindenLab.SecondLife.Quaternion)
pop
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Equals'(class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
pop
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'NotEquals'(class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
pop
ret
}
}
