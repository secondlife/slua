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
.locals init (int32, float32, class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Quaternion, string, valuetype [ScriptTypes]LindenLab.SecondLife.Key, class [mscorlib]System.Collections.ArrayList)
ldc.i4.0
stloc.s 0
ldc.r8 (00 00 00 00 00 00 00 00)
stloc.s 1
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
stloc.s 2
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 f0 3f)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
stloc.s 3
ldstr ""
stloc.s 4
ldstr ""
call valuetype [ScriptTypes]LindenLab.SecondLife.Key class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateKey'(string)
stloc.s 5
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
stloc.s 6
ldc.i4 1
ldc.i4 1
add
pop
ldc.i4 1
ldloc.s 6
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Append(int32, class [mscorlib]System.Collections.ArrayList)
pop
ldloc.s 6
ldc.i4 1
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(class [mscorlib]System.Collections.ArrayList, object)
pop
ldc.r8 (00 00 00 00 00 00 00 40)
ldc.r8 (00 00 00 00 00 00 00 40)
add
pop
ldloc.s 2
ldloc.s 2
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Add'(class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
pop
ldloc.s 3
ldloc.s 3
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Add'(class [ScriptTypes]LindenLab.SecondLife.Quaternion, class [ScriptTypes]LindenLab.SecondLife.Quaternion)
pop
ldloc.s 4
ldloc.s 4
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Add'(string, string)
pop
ldloc.s 6
ldloc.s 6
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Append(class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
pop
ldc.i4 2
ldc.i4 2
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Subtract'(int32, int32)
pop
ldc.r8 (00 00 00 00 00 00 00 40)
ldc.i4 2
conv.r8
call float64 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Subtract'(float64, float64)
pop
ldloc.s 2
ldloc.s 2
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Subtract'(class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
pop
ldloc.s 3
ldloc.s 3
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Subtract'(class [ScriptTypes]LindenLab.SecondLife.Quaternion, class [ScriptTypes]LindenLab.SecondLife.Quaternion)
pop
ldloc.s 0
ldloc.s 0
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Divide'(int32, int32)
pop
ldloc.s 1
ldloc.s 1
call float64 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Divide'(float64, float64)
pop
ldloc.s 1
ldloc.s 2
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Divide'(float32, class [ScriptTypes]LindenLab.SecondLife.Vector)
pop
ldloc.s 3
ldloc.s 2
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Divide'(class [ScriptTypes]LindenLab.SecondLife.Quaternion, class [ScriptTypes]LindenLab.SecondLife.Vector)
pop
ldloc.s 3
ldloc.s 3
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Divide'(class [ScriptTypes]LindenLab.SecondLife.Quaternion, class [ScriptTypes]LindenLab.SecondLife.Quaternion)
pop
ldloc.s 0
ldloc.s 0
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Modulo'(int32, int32)
pop
ldloc.s 2
ldloc.s 2
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Modulo'(class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
pop
ldloc.s 0
ldloc.s 0
cgt
ldc.i4.0
ceq
pop
ldloc.s 1
ldloc.s 0
conv.r8
cgt
ldc.i4.0
ceq
pop
ldloc.s 0
ldloc.s 0
clt
ldc.i4.0
ceq
pop
ldloc.s 0
ldloc.s 0
clt
ldc.i4.0
ceq
pop
ldloc.s 0
ldloc.s 0
cgt
pop
ldloc.s 1
ldloc.s 0
conv.r8
cgt
pop
ldloc.s 0
ldloc.s 0
clt
pop
ldloc.s 1
ldloc.s 0
conv.r8
clt
pop
ldloc.s 0
ldloc.s 0
ceq
pop
ldloc.s 1
ldloc.s 1
ceq
pop
ldloc.s 4
ldloc.s 4
call bool valuetype [mscorlib]System.String::op_Equality(string, string)
pop
ldloc.s 5
ldloc.s 5
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Equals'(valuetype [ScriptTypes]LindenLab.SecondLife.Key, valuetype [ScriptTypes]LindenLab.SecondLife.Key)
pop
ldloc.s 5
ldloc.s 4
call valuetype [ScriptTypes]LindenLab.SecondLife.Key class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateKey'(string)
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Equals'(valuetype [ScriptTypes]LindenLab.SecondLife.Key, valuetype [ScriptTypes]LindenLab.SecondLife.Key)
pop
ldloc.s 4
ldloc.s 5
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ToString'(valuetype [ScriptTypes]LindenLab.SecondLife.Key)
call bool valuetype [mscorlib]System.String::op_Equality(string, string)
pop
ldloc.s 2
ldloc.s 2
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Equals'(class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
pop
ldloc.s 3
ldloc.s 3
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Equals'(class [ScriptTypes]LindenLab.SecondLife.Quaternion, class [ScriptTypes]LindenLab.SecondLife.Quaternion)
pop
ldloc.s 6
ldloc.s 6
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Equals'(class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
pop
ldloc.s 0
ldloc.s 0
ceq
ldc.i4.0
ceq
pop
ldloc.s 1
ldloc.s 1
ceq
ldc.i4.0
ceq
pop
ldloc.s 4
ldloc.s 4
call bool valuetype [mscorlib]System.String::op_Equality(string, string)
ldc.i4.0
ceq
pop
ldloc.s 5
ldloc.s 5
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Equals'(valuetype [ScriptTypes]LindenLab.SecondLife.Key, valuetype [ScriptTypes]LindenLab.SecondLife.Key)
ldc.i4.0
ceq
pop
ldloc.s 5
ldloc.s 4
call valuetype [ScriptTypes]LindenLab.SecondLife.Key class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateKey'(string)
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Equals'(valuetype [ScriptTypes]LindenLab.SecondLife.Key, valuetype [ScriptTypes]LindenLab.SecondLife.Key)
ldc.i4.0
ceq
pop
ldloc.s 4
ldloc.s 5
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ToString'(valuetype [ScriptTypes]LindenLab.SecondLife.Key)
call bool valuetype [mscorlib]System.String::op_Equality(string, string)
ldc.i4.0
ceq
pop
ldloc.s 2
ldloc.s 2
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Equals'(class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
ldc.i4.0
ceq
pop
ldloc.s 3
ldloc.s 3
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Equals'(class [ScriptTypes]LindenLab.SecondLife.Quaternion, class [ScriptTypes]LindenLab.SecondLife.Quaternion)
ldc.i4.0
ceq
pop
ldloc.s 6
ldloc.s 6
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'NotEquals'(class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
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
ldloc.s 1
ldloc.s 0
conv.r8
mul
call int32 class [LslLibrary]LindenLab.SecondLife.LslRunTime::ToInteger(float32)
dup
stloc.s 0
pop
ret
}
}
