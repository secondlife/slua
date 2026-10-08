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
.locals init (valuetype [ScriptTypes]LindenLab.SecondLife.Key)
ldstr ""
call valuetype [ScriptTypes]LindenLab.SecondLife.Key class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateKey'(string)
stloc.s 0
ldc.i4 1
conv.r8
pop
ldc.i4 1
call string class [mscorlib]System.Convert::ToString(int32)
pop
ldc.i4 1
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList(object)
pop
ldc.r8 (00 00 00 00 00 00 f0 3f)
call int32 class [LslLibrary]LindenLab.SecondLife.LslRunTime::ToInteger(float32)
pop
ldc.r8 (00 00 00 00 00 00 f0 3f)
call string class [LslLibrary]LindenLab.SecondLife.LslRunTime::'ToString'(float32)
pop
ldc.r8 (00 00 00 00 00 00 f0 3f)
box [mscorlib]System.Single
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList(object)
pop
ldstr "foo"
call int32 class [LslLibrary]LindenLab.SecondLife.LslRunTime::StringToInt(string)
pop
ldstr "foo"
call float32 class [LslLibrary]LindenLab.SecondLife.LslRunTime::StringToFloat(string)
pop
ldstr "foo"
call valuetype [ScriptTypes]LindenLab.SecondLife.Key class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateKey'(string)
pop
ldstr "foo"
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ParseVector'(string)
pop
ldstr "foo"
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ParseQuaternion'(string)
pop
ldstr "foo"
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList(object)
pop
ldloc.s 0
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ToString'(valuetype [ScriptTypes]LindenLab.SecondLife.Key)
pop
ldloc.s 0
box [ScriptTypes]LindenLab.SecondLife.Key
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList(object)
pop
ldc.r8 (00 00 00 00 00 00 f0 3f)
ldc.r8 (00 00 00 00 00 00 00 40)
ldc.r8 (00 00 00 00 00 00 08 40)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ToString'(valuetype [ScriptTypes]LindenLab.SecondLife.Vector)
pop
ldc.r8 (00 00 00 00 00 00 f0 3f)
ldc.r8 (00 00 00 00 00 00 00 40)
ldc.r8 (00 00 00 00 00 00 08 40)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
box [ScriptTypes]LindenLab.SecondLife.Vector
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList(object)
pop
ldc.r8 (00 00 00 00 00 00 f0 3f)
ldc.r8 (00 00 00 00 00 00 00 40)
ldc.r8 (00 00 00 00 00 00 08 40)
ldc.r8 (00 00 00 00 00 00 10 40)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ToString'(valuetype [ScriptTypes]LindenLab.SecondLife.Quaternion)
pop
ldc.r8 (00 00 00 00 00 00 f0 3f)
ldc.r8 (00 00 00 00 00 00 00 40)
ldc.r8 (00 00 00 00 00 00 08 40)
ldc.r8 (00 00 00 00 00 00 10 40)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
box [ScriptTypes]LindenLab.SecondLife.Quaternion
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList(object)
pop
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call string class [LslLibrary]LindenLab.SecondLife.LslRunTime::ListToString(class [mscorlib]System.Collections.ArrayList)
pop
ldc.i4 1
pop
ldc.r8 (00 00 00 00 00 00 f0 3f)
pop
ldstr "foo"
pop
call valuetype [ScriptTypes]LindenLab.SecondLife.Key class [LslLibrary]LindenLab.SecondLife.Library::'llGetKey'()
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
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
pop
ldc.i4 -1
pop
ret
}
}
