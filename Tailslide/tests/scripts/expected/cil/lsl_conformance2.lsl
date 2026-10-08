.assembly extern mscorlib {.ver 1:0:5000:0}
.assembly extern LslLibrary {.ver 0:1:0:0}
.assembly extern LslUserScript {.ver 0:1:0:0}
.assembly extern ScriptTypes {.ver 0:1:0:0}
.assembly 'LSL_00000000_0000_0000_0000_000000000000' {.ver 0:0:0:0}
.class public auto ansi serializable beforefieldinit LSL_00000000_0000_0000_0000_000000000000 extends class [LslUserScript]LindenLab.SecondLife.LslUserScript
{
.field public int32 'gTestsPassed'
.field public int32 'gTestsFailed'
.field public valuetype [ScriptTypes]LindenLab.SecondLife.Key 'gNullKey'
.field public valuetype [ScriptTypes]LindenLab.SecondLife.Key 'gStringInAKey'
.field public string 'gKeyInAString'
.field public int32 'gVisitedStateTest'
.field public int32 'chat'
.field public class [ScriptTypes]LindenLab.SecondLife.Vector 'gVector'
.method public hidebysig specialname rtspecialname instance default void .ctor () cil managed
{
.maxstack 500
ldarg.0
ldc.i4 0
stfld int32 LSL_00000000_0000_0000_0000_000000000000::'gTestsPassed'
ldarg.0
ldc.i4 0
stfld int32 LSL_00000000_0000_0000_0000_000000000000::'gTestsFailed'
ldarg.0
ldstr "00000000-0000-0000-0000-000000000000"
call valuetype [ScriptTypes]LindenLab.SecondLife.Key class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateKey'(string)
stfld valuetype [ScriptTypes]LindenLab.SecondLife.Key LSL_00000000_0000_0000_0000_000000000000::'gNullKey'
ldarg.0
ldstr "foo"
call valuetype [ScriptTypes]LindenLab.SecondLife.Key class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateKey'(string)
stfld valuetype [ScriptTypes]LindenLab.SecondLife.Key LSL_00000000_0000_0000_0000_000000000000::'gStringInAKey'
ldarg.0
ldarg.0
ldfld valuetype [ScriptTypes]LindenLab.SecondLife.Key LSL_00000000_0000_0000_0000_000000000000::'gNullKey'
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ToString'(valuetype [ScriptTypes]LindenLab.SecondLife.Key)
stfld string LSL_00000000_0000_0000_0000_000000000000::'gKeyInAString'
ldarg.0
ldc.i4 0
stfld int32 LSL_00000000_0000_0000_0000_000000000000::'gVisitedStateTest'
ldarg.0
ldc.i4 1
stfld int32 LSL_00000000_0000_0000_0000_000000000000::'chat'
ldarg.0
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
stfld class [ScriptTypes]LindenLab.SecondLife.Vector LSL_00000000_0000_0000_0000_000000000000::'gVector'
ldarg.0
call instance void class [LslUserScript]LindenLab.SecondLife.LslUserScript::.ctor()
ret
}
.method public hidebysig instance default void 'gtestPassed'(string 'description', string 'actual', string 'expected') cil managed
{
.maxstack 500
ldarg.0
ldarg.0
ldfld int32 LSL_00000000_0000_0000_0000_000000000000::'gTestsPassed'
ldc.i4.1
add
stfld int32 LSL_00000000_0000_0000_0000_000000000000::'gTestsPassed'
ldarg.0
ldfld int32 LSL_00000000_0000_0000_0000_000000000000::'gTestsPassed'
pop
ret
}
.method public hidebysig instance default void 'gtestFailed'(string 'description', string 'actual', string 'expected') cil managed
{
.maxstack 500
ldarg.0
ldarg.0
ldfld int32 LSL_00000000_0000_0000_0000_000000000000::'gTestsFailed'
ldc.i4.1
add
stfld int32 LSL_00000000_0000_0000_0000_000000000000::'gTestsFailed'
ldarg.0
ldfld int32 LSL_00000000_0000_0000_0000_000000000000::'gTestsFailed'
pop
ldc.i4 0
ldstr ")"
ldarg.s 'expected'
ldstr " expected "
ldarg.s 'actual'
ldstr " ("
ldarg.s 'description'
ldstr "FAILED!: "
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Add'(string, string)
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Add'(string, string)
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Add'(string, string)
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Add'(string, string)
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Add'(string, string)
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Add'(string, string)
call void class [LslLibrary]LindenLab.SecondLife.Library::'llSay'(int32, string)
ret
}
.method public hidebysig instance default void 'gensureTrue'(string 'description', int32 'actual') cil managed
{
.maxstack 500
ldarg.s 'actual'
brfalse LabelTempJump0
ldarg.0
ldarg.s 'description'
ldarg.s 'actual'
call string class [mscorlib]System.Convert::ToString(int32)
ldc.i4 1
call string class [mscorlib]System.Convert::ToString(int32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestPassed'(string, string, string)
br LabelTempJump1
LabelTempJump0:
ldarg.0
ldarg.s 'description'
ldarg.s 'actual'
call string class [mscorlib]System.Convert::ToString(int32)
ldc.i4 1
call string class [mscorlib]System.Convert::ToString(int32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestFailed'(string, string, string)
LabelTempJump1:
ret
}
.method public hidebysig instance default void 'gensureFalse'(string 'description', int32 'actual') cil managed
{
.maxstack 500
ldarg.s 'actual'
brfalse LabelTempJump2
ldarg.0
ldarg.s 'description'
ldarg.s 'actual'
call string class [mscorlib]System.Convert::ToString(int32)
ldc.i4 0
call string class [mscorlib]System.Convert::ToString(int32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestFailed'(string, string, string)
br LabelTempJump3
LabelTempJump2:
ldarg.0
ldarg.s 'description'
ldarg.s 'actual'
call string class [mscorlib]System.Convert::ToString(int32)
ldc.i4 0
call string class [mscorlib]System.Convert::ToString(int32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestPassed'(string, string, string)
LabelTempJump3:
ret
}
.method public hidebysig instance default void 'gensureIntegerEqual'(string 'description', int32 'actual', int32 'expected') cil managed
{
.maxstack 500
ldarg.s 'expected'
ldarg.s 'actual'
ceq
brfalse LabelTempJump4
ldarg.0
ldarg.s 'description'
ldarg.s 'actual'
call string class [mscorlib]System.Convert::ToString(int32)
ldarg.s 'expected'
call string class [mscorlib]System.Convert::ToString(int32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestPassed'(string, string, string)
br LabelTempJump5
LabelTempJump4:
ldarg.0
ldarg.s 'description'
ldarg.s 'actual'
call string class [mscorlib]System.Convert::ToString(int32)
ldarg.s 'expected'
call string class [mscorlib]System.Convert::ToString(int32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestFailed'(string, string, string)
LabelTempJump5:
ret
}
.method public hidebysig instance default int32 'gfloatEqual'(float32 'actual', float32 'expected') cil managed
{
.maxstack 500
.locals init (float32, float32)
ldarg.s 'actual'
ldarg.s 'expected'
call float64 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Subtract'(float64, float64)
call float32 class [LslLibrary]LindenLab.SecondLife.Library::'llFabs'(float32)
stloc.s 0
ldc.r8 (00 00 00 e0 4d 62 50 3f)
stloc.s 1
ldloc.s 1
ldloc.s 0
clt
brfalse LabelTempJump6
ldc.i4 0
ldloc.s 0
call string class [LslLibrary]LindenLab.SecondLife.LslRunTime::'ToString'(float32)
ldstr "Float equality delta "
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Add'(string, string)
call void class [LslLibrary]LindenLab.SecondLife.Library::'llSay'(int32, string)
ldc.i4 0
ret
LabelTempJump6:
ldc.i4 1
ret
}
.method public hidebysig instance default void 'gensureFloatEqual'(string 'description', float32 'actual', float32 'expected') cil managed
{
.maxstack 500
ldarg.0
ldarg.s 'actual'
ldarg.s 'expected'
call instance int32 class LSL_00000000_0000_0000_0000_000000000000::'gfloatEqual'(float32, float32)
brfalse LabelTempJump7
ldarg.0
ldarg.s 'description'
ldarg.s 'actual'
call string class [LslLibrary]LindenLab.SecondLife.LslRunTime::'ToString'(float32)
ldarg.s 'expected'
call string class [LslLibrary]LindenLab.SecondLife.LslRunTime::'ToString'(float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestPassed'(string, string, string)
br LabelTempJump8
LabelTempJump7:
ldarg.0
ldarg.s 'description'
ldarg.s 'actual'
call string class [LslLibrary]LindenLab.SecondLife.LslRunTime::'ToString'(float32)
ldarg.s 'expected'
call string class [LslLibrary]LindenLab.SecondLife.LslRunTime::'ToString'(float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestFailed'(string, string, string)
LabelTempJump8:
ret
}
.method public hidebysig instance default void 'gensureFloatExactEqual'(string 'description', float32 'actual', float32 'expected') cil managed
{
.maxstack 500
ldarg.s 'expected'
ldarg.s 'actual'
ceq
brfalse LabelTempJump9
ldarg.0
ldarg.s 'description'
ldarg.s 'actual'
call string class [LslLibrary]LindenLab.SecondLife.LslRunTime::'ToString'(float32)
ldarg.s 'expected'
call string class [LslLibrary]LindenLab.SecondLife.LslRunTime::'ToString'(float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestPassed'(string, string, string)
br LabelTempJump10
LabelTempJump9:
ldarg.0
ldarg.s 'description'
ldarg.s 'actual'
call string class [LslLibrary]LindenLab.SecondLife.LslRunTime::'ToString'(float32)
ldarg.s 'expected'
call string class [LslLibrary]LindenLab.SecondLife.LslRunTime::'ToString'(float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestFailed'(string, string, string)
LabelTempJump10:
ret
}
.method public hidebysig instance default void 'gensureStringEqual'(string 'description', string 'actual', string 'expected') cil managed
{
.maxstack 500
ldarg.s 'expected'
ldarg.s 'actual'
call bool valuetype [mscorlib]System.String::op_Equality(string, string)
brfalse LabelTempJump11
ldarg.0
ldarg.s 'description'
ldarg.s 'actual'
ldarg.s 'expected'
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestPassed'(string, string, string)
br LabelTempJump12
LabelTempJump11:
ldarg.0
ldarg.s 'description'
ldarg.s 'actual'
ldarg.s 'expected'
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestFailed'(string, string, string)
LabelTempJump12:
ret
}
.method public hidebysig instance default void 'gensureKeyEqual'(string 'description', valuetype [ScriptTypes]LindenLab.SecondLife.Key 'actual', valuetype [ScriptTypes]LindenLab.SecondLife.Key 'expected') cil managed
{
.maxstack 500
ldarg.s 'expected'
ldarg.s 'actual'
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Equals'(valuetype [ScriptTypes]LindenLab.SecondLife.Key, valuetype [ScriptTypes]LindenLab.SecondLife.Key)
brfalse LabelTempJump13
ldarg.0
ldarg.s 'description'
ldarg.s 'actual'
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ToString'(valuetype [ScriptTypes]LindenLab.SecondLife.Key)
ldarg.s 'expected'
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ToString'(valuetype [ScriptTypes]LindenLab.SecondLife.Key)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestPassed'(string, string, string)
br LabelTempJump14
LabelTempJump13:
ldarg.0
ldarg.s 'description'
ldarg.s 'actual'
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ToString'(valuetype [ScriptTypes]LindenLab.SecondLife.Key)
ldarg.s 'expected'
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ToString'(valuetype [ScriptTypes]LindenLab.SecondLife.Key)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestFailed'(string, string, string)
LabelTempJump14:
ret
}
.method public hidebysig instance default void 'gensureVectorEqual'(string 'description', class [ScriptTypes]LindenLab.SecondLife.Vector 'actual', class [ScriptTypes]LindenLab.SecondLife.Vector 'expected') cil managed
{
.maxstack 500
ldarg.0
ldarga.s 'actual'
ldfld float32 class [ScriptTypes]LindenLab.SecondLife.Vector::z
ldarga.s 'expected'
ldfld float32 class [ScriptTypes]LindenLab.SecondLife.Vector::z
call instance int32 class LSL_00000000_0000_0000_0000_000000000000::'gfloatEqual'(float32, float32)
ldc.i4.0
ceq
ldarg.0
ldarga.s 'actual'
ldfld float32 class [ScriptTypes]LindenLab.SecondLife.Vector::y
ldarga.s 'expected'
ldfld float32 class [ScriptTypes]LindenLab.SecondLife.Vector::y
call instance int32 class LSL_00000000_0000_0000_0000_000000000000::'gfloatEqual'(float32, float32)
ldc.i4.0
ceq
ldarg.0
ldarga.s 'actual'
ldfld float32 class [ScriptTypes]LindenLab.SecondLife.Vector::x
ldarga.s 'expected'
ldfld float32 class [ScriptTypes]LindenLab.SecondLife.Vector::x
call instance int32 class LSL_00000000_0000_0000_0000_000000000000::'gfloatEqual'(float32, float32)
ldc.i4.0
ceq
or
ldc.i4.0
ceq
ldc.i4.0
ceq
or
ldc.i4.0
ceq
brfalse LabelTempJump15
ldarg.0
ldarg.s 'description'
ldarg.s 'actual'
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ToString'(valuetype [ScriptTypes]LindenLab.SecondLife.Vector)
ldarg.s 'expected'
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ToString'(valuetype [ScriptTypes]LindenLab.SecondLife.Vector)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestPassed'(string, string, string)
br LabelTempJump16
LabelTempJump15:
ldarg.0
ldarg.s 'description'
ldarg.s 'actual'
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ToString'(valuetype [ScriptTypes]LindenLab.SecondLife.Vector)
ldarg.s 'expected'
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ToString'(valuetype [ScriptTypes]LindenLab.SecondLife.Vector)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestFailed'(string, string, string)
LabelTempJump16:
ret
}
.method public hidebysig instance default void 'gensureRotationEqual'(string 'description', class [ScriptTypes]LindenLab.SecondLife.Quaternion 'actual', class [ScriptTypes]LindenLab.SecondLife.Quaternion 'expected') cil managed
{
.maxstack 500
ldarg.0
ldarga.s 'actual'
ldfld float32 class [ScriptTypes]LindenLab.SecondLife.Quaternion::s
ldarga.s 'expected'
ldfld float32 class [ScriptTypes]LindenLab.SecondLife.Quaternion::s
call instance int32 class LSL_00000000_0000_0000_0000_000000000000::'gfloatEqual'(float32, float32)
ldc.i4.0
ceq
ldarg.0
ldarga.s 'actual'
ldfld float32 class [ScriptTypes]LindenLab.SecondLife.Quaternion::z
ldarga.s 'expected'
ldfld float32 class [ScriptTypes]LindenLab.SecondLife.Quaternion::z
call instance int32 class LSL_00000000_0000_0000_0000_000000000000::'gfloatEqual'(float32, float32)
ldc.i4.0
ceq
ldarg.0
ldarga.s 'actual'
ldfld float32 class [ScriptTypes]LindenLab.SecondLife.Quaternion::y
ldarga.s 'expected'
ldfld float32 class [ScriptTypes]LindenLab.SecondLife.Quaternion::y
call instance int32 class LSL_00000000_0000_0000_0000_000000000000::'gfloatEqual'(float32, float32)
ldc.i4.0
ceq
ldarg.0
ldarga.s 'actual'
ldfld float32 class [ScriptTypes]LindenLab.SecondLife.Quaternion::x
ldarga.s 'expected'
ldfld float32 class [ScriptTypes]LindenLab.SecondLife.Quaternion::x
call instance int32 class LSL_00000000_0000_0000_0000_000000000000::'gfloatEqual'(float32, float32)
ldc.i4.0
ceq
or
ldc.i4.0
ceq
ldc.i4.0
ceq
or
ldc.i4.0
ceq
ldc.i4.0
ceq
or
ldc.i4.0
ceq
brfalse LabelTempJump17
ldarg.0
ldarg.s 'description'
ldarg.s 'actual'
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ToString'(valuetype [ScriptTypes]LindenLab.SecondLife.Quaternion)
ldarg.s 'expected'
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ToString'(valuetype [ScriptTypes]LindenLab.SecondLife.Quaternion)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestPassed'(string, string, string)
br LabelTempJump18
LabelTempJump17:
ldarg.0
ldarg.s 'description'
ldarg.s 'actual'
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ToString'(valuetype [ScriptTypes]LindenLab.SecondLife.Quaternion)
ldarg.s 'expected'
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ToString'(valuetype [ScriptTypes]LindenLab.SecondLife.Quaternion)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestFailed'(string, string, string)
LabelTempJump18:
ret
}
.method public hidebysig instance default void 'gensureListEqual'(string 'description', class [mscorlib]System.Collections.ArrayList 'actual', class [mscorlib]System.Collections.ArrayList 'expected') cil managed
{
.maxstack 500
ldarg.s 'expected'
ldarg.s 'actual'
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Equals'(class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
brfalse LabelTempJump19
ldarg.0
ldarg.s 'description'
ldarg.s 'actual'
call string class [LslLibrary]LindenLab.SecondLife.LslRunTime::ListToString(class [mscorlib]System.Collections.ArrayList)
ldarg.s 'expected'
call string class [LslLibrary]LindenLab.SecondLife.LslRunTime::ListToString(class [mscorlib]System.Collections.ArrayList)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestPassed'(string, string, string)
br LabelTempJump20
LabelTempJump19:
ldarg.0
ldarg.s 'description'
ldarg.s 'actual'
call string class [LslLibrary]LindenLab.SecondLife.LslRunTime::ListToString(class [mscorlib]System.Collections.ArrayList)
ldarg.s 'expected'
call string class [LslLibrary]LindenLab.SecondLife.LslRunTime::ListToString(class [mscorlib]System.Collections.ArrayList)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestFailed'(string, string, string)
LabelTempJump20:
ret
}
.method public hidebysig instance default float32 'gtestReturnFloat'() cil managed
{
.maxstack 500
ldc.r8 (00 00 00 00 00 00 f0 3f)
ret
}
.method public hidebysig instance default valuetype [ScriptTypes]LindenLab.SecondLife.Key 'gtestReturnKey'() cil managed
{
.maxstack 500
ldstr "00000000-0000-0000-0000-000000000000"
call valuetype [ScriptTypes]LindenLab.SecondLife.Key class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateKey'(string)
ret
}
.method public hidebysig instance default void 'gtestArgumentAccessor'(class [ScriptTypes]LindenLab.SecondLife.Vector 'v') cil managed
{
.maxstack 500
ldc.i4 0
conv.r8
ldc.i4 0
conv.r8
ldc.i4 0
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
dup
starg.s 'v'
pop
ldarga.s 'v'
ldarg.0
call instance float32 class LSL_00000000_0000_0000_0000_000000000000::'gtestReturnFloat'()
stfld float32 class [ScriptTypes]LindenLab.SecondLife.Vector::x
ldarga.s 'v'
ldfld float32 class [ScriptTypes]LindenLab.SecondLife.Vector::x
pop
ldarga.s 'v'
ldarg.0
call instance float32 class LSL_00000000_0000_0000_0000_000000000000::'gtestReturnFloat'()
stfld float32 class [ScriptTypes]LindenLab.SecondLife.Vector::y
ldarga.s 'v'
ldfld float32 class [ScriptTypes]LindenLab.SecondLife.Vector::y
pop
ldarga.s 'v'
ldarg.0
call instance float32 class LSL_00000000_0000_0000_0000_000000000000::'gtestReturnFloat'()
stfld float32 class [ScriptTypes]LindenLab.SecondLife.Vector::z
ldarga.s 'v'
ldfld float32 class [ScriptTypes]LindenLab.SecondLife.Vector::z
pop
ldarg.0
ldstr "testArgumentAccessor"
ldarg.s 'v'
ldc.i4 1
conv.r8
ldc.i4 1
conv.r8
ldc.i4 1
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureVectorEqual'(string, class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
ret
}
.method public hidebysig instance default void 'gtestLocalAccessor'() cil managed
{
.maxstack 500
.locals init (class [ScriptTypes]LindenLab.SecondLife.Vector)
ldc.i4 0
conv.r8
ldc.i4 0
conv.r8
ldc.i4 0
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
stloc.s 0
ldloca.s 0
ldarg.0
call instance float32 class LSL_00000000_0000_0000_0000_000000000000::'gtestReturnFloat'()
stfld float32 class [ScriptTypes]LindenLab.SecondLife.Vector::x
ldloca.s 0
ldfld float32 class [ScriptTypes]LindenLab.SecondLife.Vector::x
pop
ldloca.s 0
ldarg.0
call instance float32 class LSL_00000000_0000_0000_0000_000000000000::'gtestReturnFloat'()
stfld float32 class [ScriptTypes]LindenLab.SecondLife.Vector::y
ldloca.s 0
ldfld float32 class [ScriptTypes]LindenLab.SecondLife.Vector::y
pop
ldloca.s 0
ldarg.0
call instance float32 class LSL_00000000_0000_0000_0000_000000000000::'gtestReturnFloat'()
stfld float32 class [ScriptTypes]LindenLab.SecondLife.Vector::z
ldloca.s 0
ldfld float32 class [ScriptTypes]LindenLab.SecondLife.Vector::z
pop
ldarg.0
ldstr "testLocalAccessor"
ldloc.s 0
ldc.i4 1
conv.r8
ldc.i4 1
conv.r8
ldc.i4 1
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureVectorEqual'(string, class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
ret
}
.method public hidebysig instance default void 'gtestGlobalAccessor'() cil managed
{
.maxstack 500
ldarg.0
ldc.i4 0
conv.r8
ldc.i4 0
conv.r8
ldc.i4 0
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
stfld class [ScriptTypes]LindenLab.SecondLife.Vector LSL_00000000_0000_0000_0000_000000000000::'gVector'
ldarg.0
ldfld class [ScriptTypes]LindenLab.SecondLife.Vector LSL_00000000_0000_0000_0000_000000000000::'gVector'
pop
ldarg.0
ldflda class [ScriptTypes]LindenLab.SecondLife.Vector LSL_00000000_0000_0000_0000_000000000000::'gVector'
ldarg.0
call instance float32 class LSL_00000000_0000_0000_0000_000000000000::'gtestReturnFloat'()
stfld float32 class [ScriptTypes]LindenLab.SecondLife.Vector::x
ldarg.0
ldflda class [ScriptTypes]LindenLab.SecondLife.Vector LSL_00000000_0000_0000_0000_000000000000::'gVector'
ldfld float32 class [ScriptTypes]LindenLab.SecondLife.Vector::x
pop
ldarg.0
ldflda class [ScriptTypes]LindenLab.SecondLife.Vector LSL_00000000_0000_0000_0000_000000000000::'gVector'
ldarg.0
call instance float32 class LSL_00000000_0000_0000_0000_000000000000::'gtestReturnFloat'()
stfld float32 class [ScriptTypes]LindenLab.SecondLife.Vector::y
ldarg.0
ldflda class [ScriptTypes]LindenLab.SecondLife.Vector LSL_00000000_0000_0000_0000_000000000000::'gVector'
ldfld float32 class [ScriptTypes]LindenLab.SecondLife.Vector::y
pop
ldarg.0
ldflda class [ScriptTypes]LindenLab.SecondLife.Vector LSL_00000000_0000_0000_0000_000000000000::'gVector'
ldarg.0
call instance float32 class LSL_00000000_0000_0000_0000_000000000000::'gtestReturnFloat'()
stfld float32 class [ScriptTypes]LindenLab.SecondLife.Vector::z
ldarg.0
ldflda class [ScriptTypes]LindenLab.SecondLife.Vector LSL_00000000_0000_0000_0000_000000000000::'gVector'
ldfld float32 class [ScriptTypes]LindenLab.SecondLife.Vector::z
pop
ldarg.0
ldstr "testGlobalAccessor"
ldarg.0
ldfld class [ScriptTypes]LindenLab.SecondLife.Vector LSL_00000000_0000_0000_0000_000000000000::'gVector'
ldc.i4 1
conv.r8
ldc.i4 1
conv.r8
ldc.i4 1
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureVectorEqual'(string, class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
ret
}
.method public hidebysig instance default void 'gtests'() cil managed
{
.maxstack 500
.locals init (valuetype [ScriptTypes]LindenLab.SecondLife.Key, valuetype [ScriptTypes]LindenLab.SecondLife.Key, class [mscorlib]System.Collections.ArrayList, string, class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Quaternion, int32, float32, class [ScriptTypes]LindenLab.SecondLife.Quaternion, valuetype [ScriptTypes]LindenLab.SecondLife.Key, int32, float32, string)
ldarg.0
ldstr "gVisitedStateTest "
ldarg.0
ldfld int32 LSL_00000000_0000_0000_0000_000000000000::'gVisitedStateTest'
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureTrue'(string, int32)
ldarg.0
ldstr "1.4e-45 == (float)\"1.4e-45\""
ldc.r8 (00 00 00 00 00 00 a0 36)
ldstr "1.4e-45"
call float32 class [LslLibrary]LindenLab.SecondLife.LslRunTime::StringToFloat(string)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureFloatExactEqual'(string, float32, float32)
ldarg.0
ldc.i4 0
conv.r8
ldc.i4 0
conv.r8
ldc.i4 0
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestArgumentAccessor'(class [ScriptTypes]LindenLab.SecondLife.Vector)
ldarg.0
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestLocalAccessor'()
ldarg.0
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestGlobalAccessor'()
ldarg.0
ldstr "(string)[1, testReturnFloat(), testReturnFloat()] == \"11.01.0\""
ldc.i4 1
box [mscorlib]System.Int32
ldarg.0
call instance float32 class LSL_00000000_0000_0000_0000_000000000000::'gtestReturnFloat'()
box [mscorlib]System.Single
ldarg.0
call instance float32 class LSL_00000000_0000_0000_0000_000000000000::'gtestReturnFloat'()
box [mscorlib]System.Single
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call string class [LslLibrary]LindenLab.SecondLife.LslRunTime::ListToString(class [mscorlib]System.Collections.ArrayList)
ldstr "11.0000001.000000"
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureStringEqual'(string, string, string)
ldstr "foo"
call valuetype [ScriptTypes]LindenLab.SecondLife.Key class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateKey'(string)
stloc.s 0
ldstr "foo"
call valuetype [ScriptTypes]LindenLab.SecondLife.Key class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateKey'(string)
stloc.s 1
ldarg.0
ldstr "k = \"foo\";k2 = \"foo\";k == k2"
ldloc.s 0
ldloc.s 1
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureKeyEqual'(string, valuetype [ScriptTypes]LindenLab.SecondLife.Key, valuetype [ScriptTypes]LindenLab.SecondLife.Key)
ldarg.0
ldstr "[] == []"
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Equals'(class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
ldc.i4 1
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "[1] == [2]"
ldc.i4 2
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
ldc.i4 1
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Equals'(class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
ldc.i4 1
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "[1,2] == [1,2]"
ldc.i4 1
box [mscorlib]System.Int32
ldc.i4 2
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
ldc.i4 1
box [mscorlib]System.Int32
ldc.i4 2
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Equals'(class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
ldc.i4 1
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "[1,2] == [2]"
ldc.i4 2
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
ldc.i4 1
box [mscorlib]System.Int32
ldc.i4 2
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Equals'(class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
ldc.i4 0
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "[1] == [2,3]"
ldc.i4 2
box [mscorlib]System.Int32
ldc.i4 3
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
ldc.i4 1
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Equals'(class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
ldc.i4 0
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "[] != []"
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'NotEquals'(class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
ldc.i4 0
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "[1] != [2]"
ldc.i4 2
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
ldc.i4 1
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'NotEquals'(class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
ldc.i4 0
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "[1,2] != [1,2]"
ldc.i4 1
box [mscorlib]System.Int32
ldc.i4 2
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
ldc.i4 1
box [mscorlib]System.Int32
ldc.i4 2
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'NotEquals'(class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
ldc.i4 0
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "[1,2] != [2]"
ldc.i4 2
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
ldc.i4 1
box [mscorlib]System.Int32
ldc.i4 2
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'NotEquals'(class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
ldc.i4 1
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "[1] != [2,3]"
ldc.i4 2
box [mscorlib]System.Int32
ldc.i4 3
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
ldc.i4 1
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'NotEquals'(class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
ldc.i4 1
neg
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(float)\"5.0a\" == 5.0"
ldstr "5a"
call float32 class [LslLibrary]LindenLab.SecondLife.LslRunTime::StringToFloat(string)
ldc.r8 (00 00 00 00 00 00 14 40)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureFloatEqual'(string, float32, float32)
ldarg.0
ldstr "(integer)\"12foo\" == 12"
ldstr "12foo"
call int32 class [LslLibrary]LindenLab.SecondLife.LslRunTime::StringToInt(string)
conv.r8
ldc.i4 12
conv.r8
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureFloatEqual'(string, float32, float32)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
stloc.s 2
ldc.i4 42
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
dup
stloc.s 2
pop
ldc.i4 1
ldloc.s 2
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Append(int32, class [mscorlib]System.Collections.ArrayList)
pop
ldarg.0
ldstr "list l = [42]; l + 1; l == [42]"
ldloc.s 2
ldc.i4 42
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureListEqual'(string, class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
ldc.i4 42
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
dup
stloc.s 2
pop
ldc.r8 (00 00 00 00 00 00 f8 3f)
ldloc.s 2
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Append(float32, class [mscorlib]System.Collections.ArrayList)
pop
ldarg.0
ldstr "list l = [42]; l + 1.5; l == [42]"
ldloc.s 2
ldc.i4 42
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureListEqual'(string, class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
ldc.i4 42
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
dup
stloc.s 2
pop
ldstr "00000000-0000-0000-0000-000000000000"
ldloc.s 2
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Append(string, class [mscorlib]System.Collections.ArrayList)
pop
ldarg.0
ldstr "list l = [42]; l + NULL_KEY; l == [42]"
ldloc.s 2
ldc.i4 42
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureListEqual'(string, class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
ldc.i4 42
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
dup
stloc.s 2
pop
ldc.i4 1
conv.r8
ldc.i4 2
conv.r8
ldc.i4 3
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
ldloc.s 2
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Append(valuetype [ScriptTypes]LindenLab.SecondLife.Vector, class [mscorlib]System.Collections.ArrayList)
pop
ldarg.0
ldstr "list l = [42]; l + <1,2,3>; l == [42]"
ldloc.s 2
ldc.i4 42
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureListEqual'(string, class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
ldc.i4 42
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
dup
stloc.s 2
pop
ldc.i4 1
conv.r8
ldc.i4 2
conv.r8
ldc.i4 3
conv.r8
ldc.i4 4
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
ldloc.s 2
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Append(valuetype [ScriptTypes]LindenLab.SecondLife.Quaternion, class [mscorlib]System.Collections.ArrayList)
pop
ldarg.0
ldstr "list l = [42]; l + <1,2,3>; l == [42]"
ldloc.s 2
ldc.i4 42
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureListEqual'(string, class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
ldloc.s 0
call bool class [LslUserScript]LindenLab.SecondLife.LslUserScript::'IsNonNullUuid'(valuetype [ScriptTypes]LindenLab.SecondLife.Key)
brfalse LabelTempJump21
ldarg.0
ldstr "if(k)"
ldstr "TRUE"
ldstr "FALSE"
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestFailed'(string, string, string)
br LabelTempJump22
LabelTempJump21:
ldarg.0
ldstr "if(k)"
ldstr "FALSE"
ldstr "FALSE"
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestPassed'(string, string, string)
LabelTempJump22:
ldarg.0
ldstr "k == \"foo\""
ldstr "foo"
ldloc.s 0
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ToString'(valuetype [ScriptTypes]LindenLab.SecondLife.Key)
call bool valuetype [mscorlib]System.String::op_Equality(string, string)
ldc.i4 1
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "gStringInAKey == \"foo\""
ldstr "foo"
ldarg.0
ldfld valuetype [ScriptTypes]LindenLab.SecondLife.Key LSL_00000000_0000_0000_0000_000000000000::'gStringInAKey'
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ToString'(valuetype [ScriptTypes]LindenLab.SecondLife.Key)
call bool valuetype [mscorlib]System.String::op_Equality(string, string)
ldc.i4 1
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "gNullKey == NULL_KEY"
ldstr "00000000-0000-0000-0000-000000000000"
ldarg.0
ldfld valuetype [ScriptTypes]LindenLab.SecondLife.Key LSL_00000000_0000_0000_0000_000000000000::'gNullKey'
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ToString'(valuetype [ScriptTypes]LindenLab.SecondLife.Key)
call bool valuetype [mscorlib]System.String::op_Equality(string, string)
ldc.i4 1
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldstr "00000000-0000-0000-0000-000000000000"
call valuetype [ScriptTypes]LindenLab.SecondLife.Key class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateKey'(string)
dup
stloc.s 0
pop
ldstr ""
stloc.s 3
ldloc.s 0
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ToString'(valuetype [ScriptTypes]LindenLab.SecondLife.Key)
dup
stloc.s 3
pop
ldarg.0
ldstr "k = NULL_KEY; string s; s = k;"
ldloc.s 3
ldstr "00000000-0000-0000-0000-000000000000"
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureStringEqual'(string, string, string)
ldc.i4 1
conv.r8
ldc.i4 1
conv.r8
ldc.i4 1
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
stloc.s 4
LabelTempJump23:
ldloc.s 4
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call bool class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Equals'(class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
ldc.i4.0
ceq
brfalse LabelTempJump24
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
dup
stloc.s 4
pop
br LabelTempJump23
LabelTempJump24:
ldarg.0
ldstr "while (v) { v = ZERO_VECTOR; }"
ldloc.s 4
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureVectorEqual'(string, class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
ldc.i4 2
conv.r8
ldc.i4 2
conv.r8
ldc.i4 2
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
dup
stloc.s 4
pop
LabelTempJump25:
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
dup
stloc.s 4
pop
ldloc.s 4
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call bool class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Equals'(class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
ldc.i4.0
ceq
brtrue LabelTempJump25
ldarg.0
ldstr "v = <2,2,2>; do { v = ZERO_VECTOR } while (v);"
ldloc.s 4
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureVectorEqual'(string, class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
ldc.i4 3
conv.r8
ldc.i4 3
conv.r8
ldc.i4 3
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
dup
stloc.s 4
pop
LabelTempJump26:
ldloc.s 4
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call bool class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Equals'(class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
ldc.i4.0
ceq
brfalse LabelTempJump27
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
dup
stloc.s 4
pop
br LabelTempJump26
LabelTempJump27:
ldarg.0
ldstr "for (v = <3,3,3>;v;v=ZERO_VECTOR) {}"
ldloc.s 4
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureVectorEqual'(string, class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
ldstr "7c42811e-229f-4500-b6d7-2c37324ff816"
call valuetype [ScriptTypes]LindenLab.SecondLife.Key class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateKey'(string)
dup
stloc.s 0
pop
LabelTempJump28:
ldloc.s 0
call bool class [LslUserScript]LindenLab.SecondLife.LslUserScript::'IsNonNullUuid'(valuetype [ScriptTypes]LindenLab.SecondLife.Key)
brfalse LabelTempJump29
ldstr "00000000-0000-0000-0000-000000000000"
call valuetype [ScriptTypes]LindenLab.SecondLife.Key class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateKey'(string)
dup
stloc.s 0
pop
br LabelTempJump28
LabelTempJump29:
ldarg.0
ldstr "while (k) { k = NULL_KEY; }"
ldloc.s 0
ldstr "00000000-0000-0000-0000-000000000000"
call valuetype [ScriptTypes]LindenLab.SecondLife.Key class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateKey'(string)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureKeyEqual'(string, valuetype [ScriptTypes]LindenLab.SecondLife.Key, valuetype [ScriptTypes]LindenLab.SecondLife.Key)
ldstr "7c42811e-229f-4500-b6d7-2c37324ff816"
call valuetype [ScriptTypes]LindenLab.SecondLife.Key class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateKey'(string)
dup
stloc.s 0
pop
LabelTempJump30:
ldstr "00000000-0000-0000-0000-000000000000"
call valuetype [ScriptTypes]LindenLab.SecondLife.Key class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateKey'(string)
dup
stloc.s 0
pop
ldloc.s 0
call bool class [LslUserScript]LindenLab.SecondLife.LslUserScript::'IsNonNullUuid'(valuetype [ScriptTypes]LindenLab.SecondLife.Key)
brtrue LabelTempJump30
ldarg.0
ldstr "k = \"7c42811e-229f-4500-b6d7-2c37324ff816\"; do { k = NULL_KEY } while (k);"
ldloc.s 0
ldstr "00000000-0000-0000-0000-000000000000"
call valuetype [ScriptTypes]LindenLab.SecondLife.Key class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateKey'(string)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureKeyEqual'(string, valuetype [ScriptTypes]LindenLab.SecondLife.Key, valuetype [ScriptTypes]LindenLab.SecondLife.Key)
ldstr "7c42811e-229f-4500-b6d7-2c37324ff816"
call valuetype [ScriptTypes]LindenLab.SecondLife.Key class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateKey'(string)
dup
stloc.s 0
pop
LabelTempJump31:
ldloc.s 0
call bool class [LslUserScript]LindenLab.SecondLife.LslUserScript::'IsNonNullUuid'(valuetype [ScriptTypes]LindenLab.SecondLife.Key)
brfalse LabelTempJump32
ldstr "00000000-0000-0000-0000-000000000000"
call valuetype [ScriptTypes]LindenLab.SecondLife.Key class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateKey'(string)
dup
stloc.s 0
pop
br LabelTempJump31
LabelTempJump32:
ldarg.0
ldstr "for (k = \"7c42811e-229f-4500-b6d7-2c37324ff816\";k;k=NULL_KEY) {}"
ldloc.s 0
ldstr "00000000-0000-0000-0000-000000000000"
call valuetype [ScriptTypes]LindenLab.SecondLife.Key class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateKey'(string)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureKeyEqual'(string, valuetype [ScriptTypes]LindenLab.SecondLife.Key, valuetype [ScriptTypes]LindenLab.SecondLife.Key)
ldc.i4 1
conv.r8
ldc.i4 1
conv.r8
ldc.i4 1
conv.r8
ldc.i4 1
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
stloc.s 5
LabelTempJump33:
ldloc.s 5
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 f0 3f)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
call bool class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Equals'(class [ScriptTypes]LindenLab.SecondLife.Quaternion, class [ScriptTypes]LindenLab.SecondLife.Quaternion)
ldc.i4.0
ceq
brfalse LabelTempJump34
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 f0 3f)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
dup
stloc.s 5
pop
br LabelTempJump33
LabelTempJump34:
ldarg.0
ldstr "while (q) { q = ZERO_ROTATION; }"
ldloc.s 5
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 f0 3f)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureRotationEqual'(string, class [ScriptTypes]LindenLab.SecondLife.Quaternion, class [ScriptTypes]LindenLab.SecondLife.Quaternion)
ldc.i4 2
conv.r8
ldc.i4 2
conv.r8
ldc.i4 2
conv.r8
ldc.i4 2
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
dup
stloc.s 5
pop
LabelTempJump35:
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 f0 3f)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
dup
stloc.s 5
pop
ldloc.s 5
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 f0 3f)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
call bool class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Equals'(class [ScriptTypes]LindenLab.SecondLife.Quaternion, class [ScriptTypes]LindenLab.SecondLife.Quaternion)
ldc.i4.0
ceq
brtrue LabelTempJump35
ldarg.0
ldstr "q = <2,2,2>; do { v = ZERO_ROTATION } while (q);"
ldloc.s 5
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 f0 3f)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureRotationEqual'(string, class [ScriptTypes]LindenLab.SecondLife.Quaternion, class [ScriptTypes]LindenLab.SecondLife.Quaternion)
ldc.i4 3
conv.r8
ldc.i4 3
conv.r8
ldc.i4 3
conv.r8
ldc.i4 3
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
dup
stloc.s 5
pop
LabelTempJump36:
ldloc.s 5
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 f0 3f)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
call bool class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Equals'(class [ScriptTypes]LindenLab.SecondLife.Quaternion, class [ScriptTypes]LindenLab.SecondLife.Quaternion)
ldc.i4.0
ceq
brfalse LabelTempJump37
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 f0 3f)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
dup
stloc.s 5
pop
br LabelTempJump36
LabelTempJump37:
ldarg.0
ldstr "for (q = <3,3,3,3>;q;q=ZERO_ROTATION) {}"
ldloc.s 5
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 f0 3f)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureRotationEqual'(string, class [ScriptTypes]LindenLab.SecondLife.Quaternion, class [ScriptTypes]LindenLab.SecondLife.Quaternion)
ldc.i4 1
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
dup
stloc.s 2
pop
ldc.i4 2
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
dup
stloc.s 2
pop
LabelTempJump38:
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
dup
stloc.s 2
pop
ldloc.s 2
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call bool class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Equals'(class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
ldc.i4.0
ceq
brtrue LabelTempJump38
ldarg.0
ldstr "l = [2]; do { v = [] } while (l);"
ldloc.s 2
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureListEqual'(string, class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
ldstr "1!"
dup
stloc.s 3
pop
LabelTempJump39:
ldloc.s 3
ldstr ""
call bool string::op_Equality(string, string)
ldc.i4.0
ceq
brfalse LabelTempJump40
ldstr ""
dup
stloc.s 3
pop
br LabelTempJump39
LabelTempJump40:
ldarg.0
ldstr "while (s) { s = \"\"; }"
ldloc.s 3
ldstr ""
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureStringEqual'(string, string, string)
ldstr "2!"
dup
stloc.s 3
pop
LabelTempJump41:
ldstr ""
dup
stloc.s 3
pop
ldloc.s 3
ldstr ""
call bool string::op_Equality(string, string)
ldc.i4.0
ceq
brtrue LabelTempJump41
ldarg.0
ldstr "s = \"2!\"; do { s = \"\" } while (s);"
ldloc.s 3
ldstr ""
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureStringEqual'(string, string, string)
ldstr "3!"
dup
stloc.s 3
pop
LabelTempJump42:
ldloc.s 3
ldstr ""
call bool string::op_Equality(string, string)
ldc.i4.0
ceq
brfalse LabelTempJump43
ldstr ""
dup
stloc.s 3
pop
br LabelTempJump42
LabelTempJump43:
ldarg.0
ldstr "for (s = \"3!\";s;s=\"\") {}"
ldloc.s 3
ldstr ""
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureStringEqual'(string, string, string)
ldc.i4 1
stloc.s 6
LabelTempJump44:
ldloc.s 6
brfalse LabelTempJump45
ldc.i4 0
dup
stloc.s 6
pop
br LabelTempJump44
LabelTempJump45:
ldarg.0
ldstr "while (i) { i = 0; }"
ldloc.s 6
ldc.i4 0
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldc.i4 2
dup
stloc.s 6
pop
LabelTempJump46:
ldc.i4 0
dup
stloc.s 6
pop
ldloc.s 6
brtrue LabelTempJump46
ldarg.0
ldstr "i = 2; do { i = 0 } while (i);"
ldloc.s 6
ldc.i4 0
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldc.i4 3
dup
stloc.s 6
pop
LabelTempJump47:
ldloc.s 6
brfalse LabelTempJump48
ldc.i4 0
dup
stloc.s 6
pop
br LabelTempJump47
LabelTempJump48:
ldarg.0
ldstr "for (i = 3;i;i=0) {}"
ldloc.s 6
ldc.i4 0
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldc.i4 1
conv.r8
stloc.s 7
LabelTempJump49:
ldloc.s 7
ldc.r8 (00 00 00 00 00 00 00 00)
ceq
ldc.i4.0
ceq
brfalse LabelTempJump50
ldc.i4 0
conv.r8
dup
stloc.s 7
pop
br LabelTempJump49
LabelTempJump50:
ldarg.0
ldstr "while (f) { f = 0; }"
ldloc.s 7
ldc.i4 0
conv.r8
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureFloatEqual'(string, float32, float32)
ldc.i4 2
conv.r8
dup
stloc.s 7
pop
LabelTempJump51:
ldc.i4 0
conv.r8
dup
stloc.s 7
pop
ldloc.s 7
ldc.r8 (00 00 00 00 00 00 00 00)
ceq
ldc.i4.0
ceq
brtrue LabelTempJump51
ldarg.0
ldstr "f = 2; do { f = 0 } while (f);"
ldloc.s 7
ldc.i4 0
conv.r8
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureFloatEqual'(string, float32, float32)
ldc.i4 3
conv.r8
dup
stloc.s 7
pop
LabelTempJump52:
ldloc.s 7
ldc.r8 (00 00 00 00 00 00 00 00)
ceq
ldc.i4.0
ceq
brfalse LabelTempJump53
ldc.i4 0
conv.r8
dup
stloc.s 7
pop
br LabelTempJump52
LabelTempJump53:
ldarg.0
ldstr "for (f = 3;f;f=0) {}"
ldloc.s 7
ldc.i4 0
conv.r8
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureFloatEqual'(string, float32, float32)
ldc.i4 1
conv.r8
ldc.i4 2
conv.r8
ldc.i4 3
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
dup
stloc.s 4
pop
ldc.i4 2
conv.r8
ldloc.s 4
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Multiply'(float32, class [ScriptTypes]LindenLab.SecondLife.Vector)
dup
stloc.s 4
pop
ldarg.0
ldstr "v = <1,2,3>; v *= 2;"
ldloc.s 4
ldc.i4 2
conv.r8
ldc.i4 4
conv.r8
ldc.i4 6
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureVectorEqual'(string, class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
ldc.i4 1
conv.r8
ldc.i4 2
conv.r8
ldc.i4 3
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
dup
stloc.s 4
pop
ldc.r8 (00 00 00 00 00 00 e0 3f)
ldloc.s 4
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Multiply'(float32, class [ScriptTypes]LindenLab.SecondLife.Vector)
dup
stloc.s 4
pop
ldarg.0
ldstr "v = <1,2,3>; v *= 0.5;"
ldloc.s 4
ldc.r8 (00 00 00 00 00 00 e0 3f)
ldc.i4 1
conv.r8
ldc.r8 (00 00 00 00 00 00 f8 3f)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureVectorEqual'(string, class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
ldc.i4 1
conv.r8
ldc.i4 0
conv.r8
ldc.i4 0
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
dup
stloc.s 4
pop
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 f0 3f)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
ldloc.s 4
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Multiply'(class [ScriptTypes]LindenLab.SecondLife.Quaternion, class [ScriptTypes]LindenLab.SecondLife.Vector)
dup
stloc.s 4
pop
ldarg.0
ldstr "vector v = <1,0,0>; v *= ZERO_ROTATION;"
ldloc.s 4
ldc.i4 1
conv.r8
ldc.i4 0
conv.r8
ldc.i4 0
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureVectorEqual'(string, class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
ldc.i4 0
conv.r8
ldc.i4 1
conv.r8
ldc.i4 0
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
dup
stloc.s 4
pop
ldc.i4 0
conv.r8
ldc.i4 0
conv.r8
ldc.i4 1
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
ldloc.s 4
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Modulo'(class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
dup
stloc.s 4
pop
ldarg.0
ldstr "vector v = <0,1,0>; v %= <0,0,1>;"
ldloc.s 4
ldc.i4 1
conv.r8
ldc.i4 0
conv.r8
ldc.i4 0
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureVectorEqual'(string, class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 f0 3f)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
stloc.s 8
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 f0 3f)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
ldloc.s 8
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Multiply'(class [ScriptTypes]LindenLab.SecondLife.Quaternion, class [ScriptTypes]LindenLab.SecondLife.Quaternion)
dup
stloc.s 8
pop
ldarg.0
ldstr "rotation r = ZERO_ROTATION; r *= ZERO_ROTATION;"
ldloc.s 8
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 f0 3f)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureRotationEqual'(string, class [ScriptTypes]LindenLab.SecondLife.Quaternion, class [ScriptTypes]LindenLab.SecondLife.Quaternion)
ldarg.0
ldstr "(string)-0.0"
ldc.r8 (00 00 00 00 00 00 00 80)
call string class [LslLibrary]LindenLab.SecondLife.LslRunTime::'ToString'(float32)
ldstr "-0.000000"
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureStringEqual'(string, string, string)
ldarg.0
ldstr "(string)<-0.0,0.0,-0.0>"
ldc.r8 (00 00 00 00 00 00 00 00)
neg
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
neg
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ToString'(valuetype [ScriptTypes]LindenLab.SecondLife.Vector)
ldstr "<-0.00000, 0.00000, -0.00000>"
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureStringEqual'(string, string, string)
ldarg.0
ldstr "(string)<-0.0,0.0,-0.0,0.0>"
ldc.r8 (00 00 00 00 00 00 00 00)
neg
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
neg
ldc.r8 (00 00 00 00 00 00 00 00)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ToString'(valuetype [ScriptTypes]LindenLab.SecondLife.Quaternion)
ldstr "<-0.00000, 0.00000, -0.00000, 0.00000>"
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureStringEqual'(string, string, string)
ldarg.0
ldstr "llList2CSV([-0.0, <-0.0,0.0,-0.0>, <-0.0,0.0,-0.0,0.0>])"
ldc.r8 (00 00 00 00 00 00 00 00)
neg
box [mscorlib]System.Single
ldc.r8 (00 00 00 00 00 00 00 00)
neg
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
neg
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
box [ScriptTypes]LindenLab.SecondLife.Vector
ldc.r8 (00 00 00 00 00 00 00 00)
neg
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
neg
ldc.r8 (00 00 00 00 00 00 00 00)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
box [ScriptTypes]LindenLab.SecondLife.Quaternion
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call string class [LslLibrary]LindenLab.SecondLife.Library::'llList2CSV'(class [mscorlib]System.Collections.ArrayList)
ldstr "-0.000000, <-0.000000, 0.000000, -0.000000>, <-0.000000, 0.000000, -0.000000, 0.000000>"
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureStringEqual'(string, string, string)
ldarg.0
ldstr "llDumpList2String([-0.0, <-0.0,0.0,-0.0>, <-0.0,0.0,-0.0,0.0>], \" ~ \")"
ldc.r8 (00 00 00 00 00 00 00 00)
neg
box [mscorlib]System.Single
ldc.r8 (00 00 00 00 00 00 00 00)
neg
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
neg
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
box [ScriptTypes]LindenLab.SecondLife.Vector
ldc.r8 (00 00 00 00 00 00 00 00)
neg
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
neg
ldc.r8 (00 00 00 00 00 00 00 00)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
box [ScriptTypes]LindenLab.SecondLife.Quaternion
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
ldstr " ~ "
call string class [LslLibrary]LindenLab.SecondLife.Library::'llDumpList2String'(class [mscorlib]System.Collections.ArrayList, string)
ldstr "-0.000000 ~ <-0.000000, 0.000000, -0.000000> ~ <-0.000000, 0.000000, -0.000000, 0.000000>"
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureStringEqual'(string, string, string)
ldarg.0
ldstr "(string)[-0.0, <-0.0,0.0,-0.0>, <-0.0,0.0,-0.0,0.0>]"
ldc.r8 (00 00 00 00 00 00 00 00)
neg
box [mscorlib]System.Single
ldc.r8 (00 00 00 00 00 00 00 00)
neg
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
neg
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
box [ScriptTypes]LindenLab.SecondLife.Vector
ldc.r8 (00 00 00 00 00 00 00 00)
neg
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
neg
ldc.r8 (00 00 00 00 00 00 00 00)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
box [ScriptTypes]LindenLab.SecondLife.Quaternion
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call string class [LslLibrary]LindenLab.SecondLife.LslRunTime::ListToString(class [mscorlib]System.Collections.ArrayList)
ldstr "-0.000000<-0.000000, 0.000000, -0.000000><-0.000000, 0.000000, -0.000000, 0.000000>"
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureStringEqual'(string, string, string)
ldarg.0
ldstr "llList2String([-0.0], 0)"
ldc.r8 (00 00 00 00 00 00 00 00)
neg
box [mscorlib]System.Single
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
ldc.i4 0
call string class [LslLibrary]LindenLab.SecondLife.Library::'llList2String'(class [mscorlib]System.Collections.ArrayList, int32)
ldstr "-0.000000"
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureStringEqual'(string, string, string)
ldarg.0
ldstr "(string)(float)\"-0.0\""
ldstr "-0.0"
call float32 class [LslLibrary]LindenLab.SecondLife.LslRunTime::StringToFloat(string)
call string class [LslLibrary]LindenLab.SecondLife.LslRunTime::'ToString'(float32)
ldstr "-0.000000"
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureStringEqual'(string, string, string)
ldarg.0
ldstr "(string)(vector)\"<-0.0,0.0,-0.0>\""
ldstr "<-0.0,0.0,-0.0>"
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ParseVector'(string)
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ToString'(valuetype [ScriptTypes]LindenLab.SecondLife.Vector)
ldstr "<-0.00000, 0.00000, -0.00000>"
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureStringEqual'(string, string, string)
ldarg.0
ldstr "(string)(rotation)\"<-0.0,0.0,-0.0,0.0>\""
ldstr "<-0.0,0.0,-0.0,0.0>"
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ParseQuaternion'(string)
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ToString'(valuetype [ScriptTypes]LindenLab.SecondLife.Quaternion)
ldstr "<-0.00000, 0.00000, -0.00000, 0.00000>"
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureStringEqual'(string, string, string)
ldarg.0
ldstr "testReturnKey() == NULL_KEY"
ldarg.0
call instance valuetype [ScriptTypes]LindenLab.SecondLife.Key class LSL_00000000_0000_0000_0000_000000000000::'gtestReturnKey'()
ldstr "00000000-0000-0000-0000-000000000000"
call valuetype [ScriptTypes]LindenLab.SecondLife.Key class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateKey'(string)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureKeyEqual'(string, valuetype [ScriptTypes]LindenLab.SecondLife.Key, valuetype [ScriptTypes]LindenLab.SecondLife.Key)
ldstr ""
call valuetype [ScriptTypes]LindenLab.SecondLife.Key class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateKey'(string)
stloc.s 9
ldarg.0
ldstr "key defaultKey;"
ldloc.s 9
ldstr ""
call valuetype [ScriptTypes]LindenLab.SecondLife.Key class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateKey'(string)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureKeyEqual'(string, valuetype [ScriptTypes]LindenLab.SecondLife.Key, valuetype [ScriptTypes]LindenLab.SecondLife.Key)
ldc.i4.0
stloc.s 10
ldarg.0
ldstr "key defaultInteger;"
ldloc.s 10
ldc.i4 0
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldc.r8 (00 00 00 00 00 00 00 00)
stloc.s 11
ldarg.0
ldstr "key defaultInteger;"
ldloc.s 11
ldc.i4 0
conv.r8
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureFloatEqual'(string, float32, float32)
ldstr ""
stloc.s 12
ldarg.0
ldstr "string defaultString;"
ldloc.s 12
ldstr ""
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureStringEqual'(string, string, string)
ldarg.0
ldstr "chat == TRUE"
ldarg.0
ldfld int32 LSL_00000000_0000_0000_0000_000000000000::'chat'
ldc.i4 1
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ret
}
.method public hidebysig instance default void 'grunTests'() cil managed
{
.maxstack 500
call void class [LslLibrary]LindenLab.SecondLife.Library::'llResetTime'()
ldarg.0
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtests'()
ldc.i4 0
ldstr " failures"
ldarg.0
ldfld int32 LSL_00000000_0000_0000_0000_000000000000::'gTestsFailed'
call string class [mscorlib]System.Convert::ToString(int32)
ldstr " seconds with "
call float32 class [LslLibrary]LindenLab.SecondLife.Library::'llGetTime'()
call string class [LslLibrary]LindenLab.SecondLife.LslRunTime::'ToString'(float32)
ldstr " tests in "
ldarg.0
ldfld int32 LSL_00000000_0000_0000_0000_000000000000::'gTestsFailed'
ldarg.0
ldfld int32 LSL_00000000_0000_0000_0000_000000000000::'gTestsPassed'
add
call string class [mscorlib]System.Convert::ToString(int32)
ldstr "Ran "
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Add'(string, string)
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Add'(string, string)
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Add'(string, string)
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Add'(string, string)
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Add'(string, string)
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Add'(string, string)
call void class [LslLibrary]LindenLab.SecondLife.Library::'llSay'(int32, string)
ldarg.0
ldc.i4 0
stfld int32 LSL_00000000_0000_0000_0000_000000000000::'gVisitedStateTest'
ldarg.0
ldfld int32 LSL_00000000_0000_0000_0000_000000000000::'gVisitedStateTest'
pop
ldarg.0
ldc.i4 0
stfld int32 LSL_00000000_0000_0000_0000_000000000000::'gTestsPassed'
ldarg.0
ldfld int32 LSL_00000000_0000_0000_0000_000000000000::'gTestsPassed'
pop
ldarg.0
ldc.i4 0
stfld int32 LSL_00000000_0000_0000_0000_000000000000::'gTestsFailed'
ldarg.0
ldfld int32 LSL_00000000_0000_0000_0000_000000000000::'gTestsFailed'
pop
ret
}
.method public hidebysig instance default void edefaultstate_entry() cil managed
{
.maxstack 500
ldarg.0
ldstr "StateTest"
call instance void class [LslUserScript]LindenLab.SecondLife.LslUserScript::ChangeState(string)
ret
ret
}
.method public hidebysig instance default void edefaulttouch_start(int32 'total_number') cil managed
{
.maxstack 500
ldarg.0
ldstr "StateTest"
call instance void class [LslUserScript]LindenLab.SecondLife.LslUserScript::ChangeState(string)
ret
ret
}
.method public hidebysig instance default void eStateTeststate_entry() cil managed
{
.maxstack 500
ldarg.0
ldstr "gVisitedStateTest"
ldarg.0
ldfld int32 LSL_00000000_0000_0000_0000_000000000000::'gVisitedStateTest'
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureFalse'(string, int32)
ldarg.0
ldc.i4 1
stfld int32 LSL_00000000_0000_0000_0000_000000000000::'gVisitedStateTest'
ldarg.0
ldfld int32 LSL_00000000_0000_0000_0000_000000000000::'gVisitedStateTest'
pop
ldarg.0
call instance void class LSL_00000000_0000_0000_0000_000000000000::'grunTests'()
ret
}
.method public hidebysig instance default void eStateTesttouch_start(int32 'total_number') cil managed
{
.maxstack 500
ldarg.0
ldstr "default"
call instance void class [LslUserScript]LindenLab.SecondLife.LslUserScript::ChangeState(string)
ret
ret
}
}
