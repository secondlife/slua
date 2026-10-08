.assembly extern mscorlib {.ver 1:0:5000:0}
.assembly extern LslLibrary {.ver 0:1:0:0}
.assembly extern LslUserScript {.ver 0:1:0:0}
.assembly extern ScriptTypes {.ver 0:1:0:0}
.assembly 'LSL_00000000_0000_0000_0000_000000000000' {.ver 0:0:0:0}
.class public auto ansi serializable beforefieldinit LSL_00000000_0000_0000_0000_000000000000 extends class [LslUserScript]LindenLab.SecondLife.LslUserScript
{
.field public int32 'gTestsPassed'
.field public int32 'gTestsFailed'
.field public int32 'gInteger'
.field public float32 'gFloat'
.field public string 'gString'
.field public class [ScriptTypes]LindenLab.SecondLife.Vector 'gVector'
.field public class [ScriptTypes]LindenLab.SecondLife.Quaternion 'gRot'
.field public class [mscorlib]System.Collections.ArrayList 'gList'
.field public class [mscorlib]System.Collections.ArrayList 'gCallOrder'
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
ldc.i4 5
stfld int32 LSL_00000000_0000_0000_0000_000000000000::'gInteger'
ldarg.0
ldc.r8 (00 00 00 00 00 00 f8 3f)
stfld float32 LSL_00000000_0000_0000_0000_000000000000::'gFloat'
ldarg.0
ldstr "foo"
stfld string LSL_00000000_0000_0000_0000_000000000000::'gString'
ldarg.0
ldc.i4 1
conv.r8
ldc.i4 2
conv.r8
ldc.i4 3
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
stfld class [ScriptTypes]LindenLab.SecondLife.Vector LSL_00000000_0000_0000_0000_000000000000::'gVector'
ldarg.0
ldc.i4 1
conv.r8
ldc.i4 2
conv.r8
ldc.i4 3
conv.r8
ldc.i4 4
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
stfld class [ScriptTypes]LindenLab.SecondLife.Quaternion LSL_00000000_0000_0000_0000_000000000000::'gRot'
ldarg.0
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
ldc.i4 1
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Append(class [mscorlib]System.Collections.ArrayList, object)
ldc.i4 2
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Append(class [mscorlib]System.Collections.ArrayList, object)
ldc.i4 3
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Append(class [mscorlib]System.Collections.ArrayList, object)
stfld class [mscorlib]System.Collections.ArrayList LSL_00000000_0000_0000_0000_000000000000::'gList'
ldarg.0
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
stfld class [mscorlib]System.Collections.ArrayList LSL_00000000_0000_0000_0000_000000000000::'gCallOrder'
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
call void class [LslLibrary]LindenLab.SecondLife.Library::Print(string)
ldc.i4 0
ldc.i4 0
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Divide'(int32, int32)
call string class [mscorlib]System.Convert::ToString(int32)
call void class [LslLibrary]LindenLab.SecondLife.Library::'llOwnerSay'(string)
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
ldloc.s 0
call string class [LslLibrary]LindenLab.SecondLife.LslRunTime::'ToString'(float32)
ldstr "Float equality delta "
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Add'(string, string)
call void class [LslLibrary]LindenLab.SecondLife.Library::Print(string)
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
.method public hidebysig instance default void 'gensureStringEqual'(string 'description', string 'actual', string 'expected') cil managed
{
.maxstack 500
ldarg.s 'expected'
ldarg.s 'actual'
call bool valuetype [mscorlib]System.String::op_Equality(string, string)
brfalse LabelTempJump9
ldarg.0
ldarg.s 'description'
ldarg.s 'actual'
ldarg.s 'expected'
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestPassed'(string, string, string)
br LabelTempJump10
LabelTempJump9:
ldarg.0
ldarg.s 'description'
ldarg.s 'actual'
ldarg.s 'expected'
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestFailed'(string, string, string)
LabelTempJump10:
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
brfalse LabelTempJump11
ldarg.0
ldarg.s 'description'
ldarg.s 'actual'
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ToString'(valuetype [ScriptTypes]LindenLab.SecondLife.Vector)
ldarg.s 'expected'
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ToString'(valuetype [ScriptTypes]LindenLab.SecondLife.Vector)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestPassed'(string, string, string)
br LabelTempJump12
LabelTempJump11:
ldarg.0
ldarg.s 'description'
ldarg.s 'actual'
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ToString'(valuetype [ScriptTypes]LindenLab.SecondLife.Vector)
ldarg.s 'expected'
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ToString'(valuetype [ScriptTypes]LindenLab.SecondLife.Vector)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestFailed'(string, string, string)
LabelTempJump12:
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
brfalse LabelTempJump13
ldarg.0
ldarg.s 'description'
ldarg.s 'actual'
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ToString'(valuetype [ScriptTypes]LindenLab.SecondLife.Quaternion)
ldarg.s 'expected'
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ToString'(valuetype [ScriptTypes]LindenLab.SecondLife.Quaternion)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestPassed'(string, string, string)
br LabelTempJump14
LabelTempJump13:
ldarg.0
ldarg.s 'description'
ldarg.s 'actual'
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ToString'(valuetype [ScriptTypes]LindenLab.SecondLife.Quaternion)
ldarg.s 'expected'
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ToString'(valuetype [ScriptTypes]LindenLab.SecondLife.Quaternion)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestFailed'(string, string, string)
LabelTempJump14:
ret
}
.method public hidebysig instance default void 'gensureListEqual'(string 'description', class [mscorlib]System.Collections.ArrayList 'actual', class [mscorlib]System.Collections.ArrayList 'expected') cil managed
{
.maxstack 500
ldarg.s 'expected'
call string class [LslLibrary]LindenLab.SecondLife.LslRunTime::ListToString(class [mscorlib]System.Collections.ArrayList)
ldarg.s 'actual'
call string class [LslLibrary]LindenLab.SecondLife.LslRunTime::ListToString(class [mscorlib]System.Collections.ArrayList)
call bool valuetype [mscorlib]System.String::op_Equality(string, string)
ldc.i4.0
ceq
ldarg.s 'expected'
ldarg.s 'actual'
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Equals'(class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
ldc.i4.0
ceq
or
ldc.i4.0
ceq
brfalse LabelTempJump15
ldarg.0
ldarg.s 'description'
ldarg.s 'actual'
call string class [LslLibrary]LindenLab.SecondLife.LslRunTime::ListToString(class [mscorlib]System.Collections.ArrayList)
ldarg.s 'expected'
call string class [LslLibrary]LindenLab.SecondLife.LslRunTime::ListToString(class [mscorlib]System.Collections.ArrayList)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestPassed'(string, string, string)
br LabelTempJump16
LabelTempJump15:
ldarg.0
ldarg.s 'description'
ldarg.s 'actual'
call string class [LslLibrary]LindenLab.SecondLife.LslRunTime::ListToString(class [mscorlib]System.Collections.ArrayList)
ldarg.s 'expected'
call string class [LslLibrary]LindenLab.SecondLife.LslRunTime::ListToString(class [mscorlib]System.Collections.ArrayList)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestFailed'(string, string, string)
LabelTempJump16:
ret
}
.method public hidebysig instance default int32 'gcallOrderFunc'(int32 'num') cil managed
{
.maxstack 500
ldarg.0
ldarg.s 'num'
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
ldarg.0
ldfld class [mscorlib]System.Collections.ArrayList LSL_00000000_0000_0000_0000_000000000000::'gCallOrder'
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Append(class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
stfld class [mscorlib]System.Collections.ArrayList LSL_00000000_0000_0000_0000_000000000000::'gCallOrder'
ldarg.0
ldfld class [mscorlib]System.Collections.ArrayList LSL_00000000_0000_0000_0000_000000000000::'gCallOrder'
pop
ldc.i4 1
ret
}
.method public hidebysig instance default int32 'gtestReturn'() cil managed
{
.maxstack 500
ldc.i4 1
ret
}
.method public hidebysig instance default float32 'gtestReturnFloat'() cil managed
{
.maxstack 500
ldc.r8 (00 00 00 00 00 00 f0 3f)
ret
}
.method public hidebysig instance default string 'gtestReturnString'() cil managed
{
.maxstack 500
ldstr "Test string"
ret
}
.method public hidebysig instance default class [mscorlib]System.Collections.ArrayList 'gtestReturnList'() cil managed
{
.maxstack 500
ldc.i4 1
box [mscorlib]System.Int32
ldc.i4 2
box [mscorlib]System.Int32
ldc.i4 3
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
ret
}
.method public hidebysig instance default class [ScriptTypes]LindenLab.SecondLife.Vector 'gtestReturnVector'() cil managed
{
.maxstack 500
ldc.i4 1
conv.r8
ldc.i4 2
conv.r8
ldc.i4 3
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
ret
}
.method public hidebysig instance default class [ScriptTypes]LindenLab.SecondLife.Quaternion 'gtestReturnRotation'() cil managed
{
.maxstack 500
ldc.i4 1
conv.r8
ldc.i4 2
conv.r8
ldc.i4 3
conv.r8
ldc.i4 4
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
ret
}
.method public hidebysig instance default class [ScriptTypes]LindenLab.SecondLife.Vector 'gtestReturnVectorNested'() cil managed
{
.maxstack 500
ldarg.0
call instance class [ScriptTypes]LindenLab.SecondLife.Vector class LSL_00000000_0000_0000_0000_000000000000::'gtestReturnVector'()
ret
}
.method public hidebysig instance default class [ScriptTypes]LindenLab.SecondLife.Vector 'gtestReturnVectorWithLibraryCall'() cil managed
{
.maxstack 500
ldc.i4 0
conv.r8
call float32 class [LslLibrary]LindenLab.SecondLife.Library::'llSin'(float32)
pop
ldc.i4 1
conv.r8
ldc.i4 2
conv.r8
ldc.i4 3
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
ret
}
.method public hidebysig instance default class [ScriptTypes]LindenLab.SecondLife.Quaternion 'gtestReturnRotationWithLibraryCall'() cil managed
{
.maxstack 500
ldc.i4 0
conv.r8
call float32 class [LslLibrary]LindenLab.SecondLife.Library::'llSin'(float32)
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
ret
}
.method public hidebysig instance default int32 'gtestParameters'(int32 'param') cil managed
{
.maxstack 500
ldc.i4 1
ldarg.s 'param'
add
dup
starg.s 'param'
pop
ldarg.s 'param'
ret
}
.method public hidebysig instance default int32 'gtestRecursion'(int32 'param') cil managed
{
.maxstack 500
ldc.i4 0
ldarg.s 'param'
clt
ldc.i4.0
ceq
brfalse LabelTempJump17
ldc.i4 0
ret
br LabelTempJump18
LabelTempJump17:
ldarg.0
ldc.i4 1
ldarg.s 'param'
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Subtract'(int32, int32)
call instance int32 class LSL_00000000_0000_0000_0000_000000000000::'gtestRecursion'(int32)
ret
LabelTempJump18:
}
.method public hidebysig instance default string 'gtestExpressionLists'(class [mscorlib]System.Collections.ArrayList 'l') cil managed
{
.maxstack 500
ldarg.s 'l'
call string class [LslLibrary]LindenLab.SecondLife.LslRunTime::ListToString(class [mscorlib]System.Collections.ArrayList)
ldstr "foo"
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Add'(string, string)
ret
}
.method public hidebysig instance default void 'gtests'() cil managed
{
.maxstack 500
.locals init (class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList, int32, class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Quaternion, class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
ldarg.0
ldstr "TRUE"
ldc.i4 1
ldc.i4 1
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "FALSE"
ldc.i4 0
ldc.i4 0
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ceq
ldc.i4.0
ceq
brfalse LabelTempJump19
ldarg.0
ldstr "if(0.0)"
ldstr "TRUE"
ldstr "FALSE"
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestFailed'(string, string, string)
br LabelTempJump20
LabelTempJump19:
ldarg.0
ldstr "if(0.0)"
ldstr "TRUE"
ldstr "TRUE"
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestPassed'(string, string, string)
LabelTempJump20:
ldc.r8 (00 00 00 a0 f7 c6 b0 3e)
ldc.r8 (00 00 00 00 00 00 00 00)
ceq
ldc.i4.0
ceq
brfalse LabelTempJump21
ldarg.0
ldstr "if(0.000001)"
ldstr "TRUE"
ldstr "TRUE"
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestPassed'(string, string, string)
br LabelTempJump22
LabelTempJump21:
ldarg.0
ldstr "if(0.000001)"
ldstr "TRUE"
ldstr "FALSE"
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestFailed'(string, string, string)
LabelTempJump22:
ldc.r8 (00 00 00 c0 cc cc ec 3f)
ldc.r8 (00 00 00 00 00 00 00 00)
ceq
ldc.i4.0
ceq
brfalse LabelTempJump23
ldarg.0
ldstr "if(0.9)"
ldstr "TRUE"
ldstr "TRUE"
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestPassed'(string, string, string)
br LabelTempJump24
LabelTempJump23:
ldarg.0
ldstr "if(0.9)"
ldstr "TRUE"
ldstr "FALSE"
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestFailed'(string, string, string)
LabelTempJump24:
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call bool class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Equals'(class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
ldc.i4.0
ceq
brfalse LabelTempJump25
ldarg.0
ldstr "if(ZERO_VECTOR)"
ldstr "TRUE"
ldstr "FALSE"
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestFailed'(string, string, string)
br LabelTempJump26
LabelTempJump25:
ldarg.0
ldstr "if(ZERO_VECTOR)"
ldstr "TRUE"
ldstr "TRUE"
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestPassed'(string, string, string)
LabelTempJump26:
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 f0 3f)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 f0 3f)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
call bool class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Equals'(class [ScriptTypes]LindenLab.SecondLife.Quaternion, class [ScriptTypes]LindenLab.SecondLife.Quaternion)
ldc.i4.0
ceq
brfalse LabelTempJump27
ldarg.0
ldstr "if(ZERO_ROTATION)"
ldstr "TRUE"
ldstr "FALSE"
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestFailed'(string, string, string)
br LabelTempJump28
LabelTempJump27:
ldarg.0
ldstr "if(ZERO_ROTATION)"
ldstr "TRUE"
ldstr "TRUE"
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestPassed'(string, string, string)
LabelTempJump28:
ldstr "00000000-0000-0000-0000-000000000000"
ldstr ""
call bool string::op_Equality(string, string)
ldc.i4.0
ceq
brfalse LabelTempJump29
ldarg.0
ldstr "if(NULL_KEY)"
ldstr "TRUE"
ldstr "TRUE"
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestPassed'(string, string, string)
br LabelTempJump30
LabelTempJump29:
ldarg.0
ldstr "if(NULL_KEY)"
ldstr "TRUE"
ldstr "FALSE"
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestFailed'(string, string, string)
LabelTempJump30:
ldstr "00000000-0000-0000-0000-000000000000"
call valuetype [ScriptTypes]LindenLab.SecondLife.Key class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateKey'(string)
call bool class [LslUserScript]LindenLab.SecondLife.LslUserScript::'IsNonNullUuid'(valuetype [ScriptTypes]LindenLab.SecondLife.Key)
brfalse LabelTempJump31
ldarg.0
ldstr "if((key)NULL_KEY)"
ldstr "TRUE"
ldstr "FALSE"
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestFailed'(string, string, string)
br LabelTempJump32
LabelTempJump31:
ldarg.0
ldstr "if((key)NULL_KEY)"
ldstr "TRUE"
ldstr "TRUE"
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestPassed'(string, string, string)
LabelTempJump32:
ldstr ""
ldstr ""
call bool string::op_Equality(string, string)
ldc.i4.0
ceq
brfalse LabelTempJump33
ldarg.0
ldstr "if(\"\")"
ldstr "TRUE"
ldstr "FALSE"
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestFailed'(string, string, string)
br LabelTempJump34
LabelTempJump33:
ldarg.0
ldstr "if(\"\")"
ldstr "TRUE"
ldstr "TRUE"
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestPassed'(string, string, string)
LabelTempJump34:
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call bool class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Equals'(class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
ldc.i4.0
ceq
brfalse LabelTempJump35
ldarg.0
ldstr "if([])"
ldstr "TRUE"
ldstr "FALSE"
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestFailed'(string, string, string)
br LabelTempJump36
LabelTempJump35:
ldarg.0
ldstr "if([])"
ldstr "TRUE"
ldstr "TRUE"
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtestPassed'(string, string, string)
LabelTempJump36:
ldarg.0
ldstr "(TRUE == TRUE)"
ldc.i4 1
ldc.i4 1
ceq
ldc.i4 1
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(TRUE == FALSE)"
ldc.i4 0
ldc.i4 1
ceq
ldc.i4 0
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(FALSE == TRUE)"
ldc.i4 1
ldc.i4 0
ceq
ldc.i4 0
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(FALSE == FALSE)"
ldc.i4 0
ldc.i4 0
ceq
ldc.i4 1
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(TRUE != TRUE)"
ldc.i4 1
ldc.i4 1
ceq
ldc.i4.0
ceq
ldc.i4 0
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(TRUE != FALSE)"
ldc.i4 0
ldc.i4 1
ceq
ldc.i4.0
ceq
ldc.i4 1
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(FALSE != TRUE)"
ldc.i4 1
ldc.i4 0
ceq
ldc.i4.0
ceq
ldc.i4 1
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(FALSE != FALSE)"
ldc.i4 0
ldc.i4 0
ceq
ldc.i4.0
ceq
ldc.i4 0
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(TRUE && TRUE)"
ldc.i4 1
ldc.i4.0
ceq
ldc.i4 1
ldc.i4.0
ceq
or
ldc.i4.0
ceq
ldc.i4 1
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(TRUE && FALSE)"
ldc.i4 0
ldc.i4.0
ceq
ldc.i4 1
ldc.i4.0
ceq
or
ldc.i4.0
ceq
ldc.i4 0
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(FALSE && TRUE)"
ldc.i4 1
ldc.i4.0
ceq
ldc.i4 0
ldc.i4.0
ceq
or
ldc.i4.0
ceq
ldc.i4 0
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(FALSE && FALSE)"
ldc.i4 0
ldc.i4.0
ceq
ldc.i4 0
ldc.i4.0
ceq
or
ldc.i4.0
ceq
ldc.i4 0
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(1 && 2)"
ldc.i4 2
ldc.i4.0
ceq
ldc.i4 1
ldc.i4.0
ceq
or
ldc.i4.0
ceq
ldc.i4 1
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(1 && 0)"
ldc.i4 0
ldc.i4.0
ceq
ldc.i4 1
ldc.i4.0
ceq
or
ldc.i4.0
ceq
ldc.i4 0
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(0 && 2)"
ldc.i4 2
ldc.i4.0
ceq
ldc.i4 0
ldc.i4.0
ceq
or
ldc.i4.0
ceq
ldc.i4 0
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(0 && 0)"
ldc.i4 0
ldc.i4.0
ceq
ldc.i4 0
ldc.i4.0
ceq
or
ldc.i4.0
ceq
ldc.i4 0
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(TRUE || TRUE)"
ldc.i4 1
ldc.i4 1
or
ldc.i4.0
ceq
ldc.i4.0
ceq
ldc.i4 1
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(TRUE || FALSE)"
ldc.i4 0
ldc.i4 1
or
ldc.i4.0
ceq
ldc.i4.0
ceq
ldc.i4 1
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(FALSE || TRUE)"
ldc.i4 1
ldc.i4 0
or
ldc.i4.0
ceq
ldc.i4.0
ceq
ldc.i4 1
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(FALSE || FALSE)"
ldc.i4 0
ldc.i4 0
or
ldc.i4.0
ceq
ldc.i4.0
ceq
ldc.i4 0
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(1 || 2)"
ldc.i4 2
ldc.i4 1
or
ldc.i4.0
ceq
ldc.i4.0
ceq
ldc.i4 1
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(1 || 0)"
ldc.i4 0
ldc.i4 1
or
ldc.i4.0
ceq
ldc.i4.0
ceq
ldc.i4 1
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(0 || 2)"
ldc.i4 2
ldc.i4 0
or
ldc.i4.0
ceq
ldc.i4.0
ceq
ldc.i4 1
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(0 || 0)"
ldc.i4 0
ldc.i4 0
or
ldc.i4.0
ceq
ldc.i4.0
ceq
ldc.i4 0
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(! TRUE)"
ldc.i4 1
ldc.i4.0
ceq
ldc.i4 0
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(! FALSE)"
ldc.i4 0
ldc.i4.0
ceq
ldc.i4 1
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(! 2)"
ldc.i4 2
ldc.i4.0
ceq
ldc.i4 0
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(! 0)"
ldc.i4 0
ldc.i4.0
ceq
ldc.i4 1
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(1 > 0)"
ldc.i4 0
ldc.i4 1
clt
ldc.i4 1
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(0 > 1)"
ldc.i4 1
ldc.i4 0
clt
ldc.i4 0
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(1 > 1)"
ldc.i4 1
ldc.i4 1
clt
ldc.i4 0
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(0 < 1)"
ldc.i4 1
ldc.i4 0
cgt
ldc.i4 1
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(1 < 0)"
ldc.i4 0
ldc.i4 1
cgt
ldc.i4 0
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(1 < 1)"
ldc.i4 1
ldc.i4 1
cgt
ldc.i4 0
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(1 >= 0)"
ldc.i4 0
ldc.i4 1
cgt
ldc.i4.0
ceq
ldc.i4 1
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(0 >= 1)"
ldc.i4 1
ldc.i4 0
cgt
ldc.i4.0
ceq
ldc.i4 0
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(1 >= 1)"
ldc.i4 1
ldc.i4 1
cgt
ldc.i4.0
ceq
ldc.i4 1
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(0 <= 1)"
ldc.i4 1
ldc.i4 0
clt
ldc.i4.0
ceq
ldc.i4 1
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(1 <= 0)"
ldc.i4 0
ldc.i4 1
clt
ldc.i4.0
ceq
ldc.i4 0
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(1 <= 1)"
ldc.i4 1
ldc.i4 1
clt
ldc.i4.0
ceq
ldc.i4 1
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(10 & 25)"
ldc.i4 25
ldc.i4 10
and
ldc.i4 8
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(10 | 25)"
ldc.i4 25
ldc.i4 10
or
ldc.i4 27
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "~10"
ldc.i4 10
not
ldc.i4 11
neg
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(10 ^ 25)"
ldc.i4 25
ldc.i4 10
xor
ldc.i4 19
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(523 >> 2)"
ldc.i4 2
ldc.i4 523
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ShiftRight'(int32, int32)
ldc.i4 130
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(523 << 2)"
ldc.i4 2
ldc.i4 523
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ShiftLeft'(int32, int32)
ldc.i4 2092
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(1 + 1)"
ldc.i4 1
ldc.i4 1
add
ldc.i4 2
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(1 + 1.1)"
ldc.r8 (00 00 00 a0 99 99 f1 3f)
ldc.i4 1
conv.r8
add
ldc.r8 (00 00 00 c0 cc cc 00 40)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureFloatEqual'(string, float32, float32)
ldarg.0
ldstr "(1.1 + 1)"
ldc.i4 1
conv.r8
ldc.r8 (00 00 00 a0 99 99 f1 3f)
add
ldc.r8 (00 00 00 c0 cc cc 00 40)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureFloatEqual'(string, float32, float32)
ldarg.0
ldstr "(1.1 + 1.1)"
ldc.r8 (00 00 00 a0 99 99 f1 3f)
ldc.r8 (00 00 00 a0 99 99 f1 3f)
add
ldc.r8 (00 00 00 a0 99 99 01 40)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureFloatEqual'(string, float32, float32)
ldarg.0
ldstr "\"foo\" + \"bar\""
ldstr "bar"
ldstr "foo"
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Add'(string, string)
ldstr "foobar"
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureStringEqual'(string, string, string)
ldarg.0
ldstr "(<1.1, 2.2, 3.3> + <4.4, 5.5, 6.6>)"
ldc.r8 (00 00 00 a0 99 99 11 40)
ldc.r8 (00 00 00 00 00 00 16 40)
ldc.r8 (00 00 00 60 66 66 1a 40)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
ldc.r8 (00 00 00 a0 99 99 f1 3f)
ldc.r8 (00 00 00 a0 99 99 01 40)
ldc.r8 (00 00 00 60 66 66 0a 40)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Add'(class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
ldc.r8 (00 00 00 00 00 00 16 40)
ldc.r8 (00 00 00 c0 cc cc 1e 40)
ldc.r8 (00 00 00 c0 cc cc 23 40)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureVectorEqual'(string, class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
ldarg.0
ldstr "(<1.1, 2.2, 3.3, 4.4> + <4.4, 5.5, 6.6, 3.3>)"
ldc.r8 (00 00 00 a0 99 99 11 40)
ldc.r8 (00 00 00 00 00 00 16 40)
ldc.r8 (00 00 00 60 66 66 1a 40)
ldc.r8 (00 00 00 60 66 66 0a 40)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
ldc.r8 (00 00 00 a0 99 99 f1 3f)
ldc.r8 (00 00 00 a0 99 99 01 40)
ldc.r8 (00 00 00 60 66 66 0a 40)
ldc.r8 (00 00 00 a0 99 99 11 40)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Add'(class [ScriptTypes]LindenLab.SecondLife.Quaternion, class [ScriptTypes]LindenLab.SecondLife.Quaternion)
ldc.r8 (00 00 00 00 00 00 16 40)
ldc.r8 (00 00 00 c0 cc cc 1e 40)
ldc.r8 (00 00 00 c0 cc cc 23 40)
ldc.r8 (00 00 00 c0 cc cc 1e 40)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureRotationEqual'(string, class [ScriptTypes]LindenLab.SecondLife.Quaternion, class [ScriptTypes]LindenLab.SecondLife.Quaternion)
ldarg.0
ldstr "([1] + 2)"
ldc.i4 2
ldc.i4 1
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Append(int32, class [mscorlib]System.Collections.ArrayList)
ldc.i4 1
box [mscorlib]System.Int32
ldc.i4 2
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureListEqual'(string, class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
ldarg.0
ldstr "([] + 1.5)"
ldc.r8 (00 00 00 00 00 00 f8 3f)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Append(float32, class [mscorlib]System.Collections.ArrayList)
ldc.r8 (00 00 00 00 00 00 f8 3f)
box [mscorlib]System.Single
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureListEqual'(string, class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
ldarg.0
ldstr "([\"foo\"] + \"bar\")"
ldstr "bar"
ldstr "foo"
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Append(string, class [mscorlib]System.Collections.ArrayList)
ldstr "foo"
ldstr "bar"
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureListEqual'(string, class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
ldarg.0
ldstr "([] + <1,2,3>)"
ldc.i4 1
conv.r8
ldc.i4 2
conv.r8
ldc.i4 3
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Append(valuetype [ScriptTypes]LindenLab.SecondLife.Vector, class [mscorlib]System.Collections.ArrayList)
ldc.i4 1
conv.r8
ldc.i4 2
conv.r8
ldc.i4 3
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
box [ScriptTypes]LindenLab.SecondLife.Vector
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureListEqual'(string, class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
ldarg.0
ldstr "([] + <1,2,3,4>)"
ldc.i4 1
conv.r8
ldc.i4 2
conv.r8
ldc.i4 3
conv.r8
ldc.i4 4
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Append(valuetype [ScriptTypes]LindenLab.SecondLife.Quaternion, class [mscorlib]System.Collections.ArrayList)
ldc.i4 1
conv.r8
ldc.i4 2
conv.r8
ldc.i4 3
conv.r8
ldc.i4 4
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
box [ScriptTypes]LindenLab.SecondLife.Quaternion
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureListEqual'(string, class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
ldarg.0
ldstr "(1 + [2])"
ldc.i4 2
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
ldc.i4 1
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(class [mscorlib]System.Collections.ArrayList, object)
ldc.i4 1
box [mscorlib]System.Int32
ldc.i4 2
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureListEqual'(string, class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
ldarg.0
ldstr "(1.0 + [2])"
ldc.i4 2
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
ldc.r8 (00 00 00 00 00 00 f0 3f)
box [mscorlib]System.Single
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(class [mscorlib]System.Collections.ArrayList, object)
ldc.r8 (00 00 00 00 00 00 f0 3f)
box [mscorlib]System.Single
ldc.i4 2
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureListEqual'(string, class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
ldarg.0
ldstr "(1 + [2])"
ldc.i4 2
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
ldstr "one"
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(class [mscorlib]System.Collections.ArrayList, object)
ldstr "one"
ldc.i4 2
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureListEqual'(string, class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
ldarg.0
ldstr "(<1.0,1.0,1.0,1.0> + [2])"
ldc.i4 2
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
ldc.r8 (00 00 00 00 00 00 f0 3f)
ldc.r8 (00 00 00 00 00 00 f0 3f)
ldc.r8 (00 00 00 00 00 00 f0 3f)
ldc.r8 (00 00 00 00 00 00 f0 3f)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
box [ScriptTypes]LindenLab.SecondLife.Quaternion
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(class [mscorlib]System.Collections.ArrayList, object)
ldc.r8 (00 00 00 00 00 00 f0 3f)
ldc.r8 (00 00 00 00 00 00 f0 3f)
ldc.r8 (00 00 00 00 00 00 f0 3f)
ldc.r8 (00 00 00 00 00 00 f0 3f)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
box [ScriptTypes]LindenLab.SecondLife.Quaternion
ldc.i4 2
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureListEqual'(string, class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
ldarg.0
ldstr "(<1.0,1.0,1.0> + [2])"
ldc.i4 2
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
ldc.r8 (00 00 00 00 00 00 f0 3f)
ldc.r8 (00 00 00 00 00 00 f0 3f)
ldc.r8 (00 00 00 00 00 00 f0 3f)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
box [ScriptTypes]LindenLab.SecondLife.Vector
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(class [mscorlib]System.Collections.ArrayList, object)
ldc.r8 (00 00 00 00 00 00 f0 3f)
ldc.r8 (00 00 00 00 00 00 f0 3f)
ldc.r8 (00 00 00 00 00 00 f0 3f)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
box [ScriptTypes]LindenLab.SecondLife.Vector
ldc.i4 2
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureListEqual'(string, class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
stloc.s 0
ldloc.s 0
stloc.s 1
ldstr "foo"
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
ldloc.s 0
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Append(class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
dup
stloc.s 0
pop
ldarg.0
ldstr "list a = []; list b = a; a += [\"foo\"]; a == [\"foo\"]"
ldloc.s 0
ldstr "foo"
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureListEqual'(string, class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
ldarg.0
ldstr "list a = []; list b = a; a += [\"foo\"]; b == []"
ldloc.s 1
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureListEqual'(string, class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
ldstr "a"
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
dup
stloc.s 0
pop
ldstr "b"
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
dup
stloc.s 1
pop
ldloc.s 1
ldloc.s 0
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Append(class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
stloc.s 2
ldarg.0
ldstr "a = [\"a\"]; b = [\"b\"]; list c = a + b; a == [\"a\"];"
ldloc.s 0
ldstr "a"
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureListEqual'(string, class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
ldarg.0
ldstr "a = [\"a\"]; b = [\"b\"]; list c = a + b; b == [\"b\"];"
ldloc.s 1
ldstr "b"
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureListEqual'(string, class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
ldarg.0
ldstr "a = [\"a\"]; b = [\"b\"]; list c = a + b; c == [\"a\", \"b\"];"
ldloc.s 2
ldstr "a"
ldstr "b"
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureListEqual'(string, class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
ldarg.0
ldstr "(1 - 1)"
ldc.i4 1
ldc.i4 1
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Subtract'(int32, int32)
ldc.i4 0
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(1 - 0.5)"
ldc.r8 (00 00 00 00 00 00 e0 3f)
ldc.i4 1
conv.r8
call float64 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Subtract'(float64, float64)
ldc.r8 (00 00 00 00 00 00 e0 3f)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureFloatEqual'(string, float32, float32)
ldarg.0
ldstr "(1.5 - 1)"
ldc.i4 1
conv.r8
ldc.r8 (00 00 00 00 00 00 f8 3f)
call float64 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Subtract'(float64, float64)
ldc.r8 (00 00 00 00 00 00 e0 3f)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureFloatEqual'(string, float32, float32)
ldarg.0
ldstr "(2.2 - 1.1)"
ldc.r8 (00 00 00 a0 99 99 f1 3f)
ldc.r8 (00 00 00 a0 99 99 01 40)
call float64 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Subtract'(float64, float64)
ldc.r8 (00 00 00 a0 99 99 f1 3f)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureFloatEqual'(string, float32, float32)
ldarg.0
ldstr "(<1.5, 2.5, 3.5> - <4.5, 5.5, 6.5>)"
ldc.r8 (00 00 00 00 00 00 12 40)
ldc.r8 (00 00 00 00 00 00 16 40)
ldc.r8 (00 00 00 00 00 00 1a 40)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
ldc.r8 (00 00 00 00 00 00 f8 3f)
ldc.r8 (00 00 00 00 00 00 04 40)
ldc.r8 (00 00 00 00 00 00 0c 40)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Subtract'(class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
ldc.r8 (00 00 00 00 00 00 08 40)
neg
ldc.r8 (00 00 00 00 00 00 08 40)
neg
ldc.r8 (00 00 00 00 00 00 08 40)
neg
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureVectorEqual'(string, class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
ldarg.0
ldstr "(<1.5, 2.5, 3.5, 4.5> - <4.5, 5.5, 6.5, 7.5>)"
ldc.r8 (00 00 00 00 00 00 12 40)
ldc.r8 (00 00 00 00 00 00 16 40)
ldc.r8 (00 00 00 00 00 00 1a 40)
ldc.r8 (00 00 00 00 00 00 1e 40)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
ldc.r8 (00 00 00 00 00 00 f8 3f)
ldc.r8 (00 00 00 00 00 00 04 40)
ldc.r8 (00 00 00 00 00 00 0c 40)
ldc.r8 (00 00 00 00 00 00 12 40)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Subtract'(class [ScriptTypes]LindenLab.SecondLife.Quaternion, class [ScriptTypes]LindenLab.SecondLife.Quaternion)
ldc.r8 (00 00 00 00 00 00 08 40)
neg
ldc.r8 (00 00 00 00 00 00 08 40)
neg
ldc.r8 (00 00 00 00 00 00 08 40)
neg
ldc.r8 (00 00 00 00 00 00 08 40)
neg
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureRotationEqual'(string, class [ScriptTypes]LindenLab.SecondLife.Quaternion, class [ScriptTypes]LindenLab.SecondLife.Quaternion)
ldarg.0
ldstr "(2 * 3)"
ldc.i4 3
ldc.i4 2
mul
ldc.i4 6
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(2 * 3.5)"
ldc.r8 (00 00 00 00 00 00 0c 40)
ldc.i4 2
conv.r8
mul
ldc.r8 (00 00 00 00 00 00 1c 40)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureFloatEqual'(string, float32, float32)
ldarg.0
ldstr "(2.5 * 3)"
ldc.i4 3
conv.r8
ldc.r8 (00 00 00 00 00 00 04 40)
mul
ldc.r8 (00 00 00 00 00 00 1e 40)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureFloatEqual'(string, float32, float32)
ldarg.0
ldstr "(2.5 * 3.5)"
ldc.r8 (00 00 00 00 00 00 0c 40)
ldc.r8 (00 00 00 00 00 00 04 40)
mul
ldc.r8 (00 00 00 00 00 80 21 40)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureFloatEqual'(string, float32, float32)
ldarg.0
ldstr "(<1.1, 2.2, 3.3> * 2)"
ldc.i4 2
conv.r8
ldc.r8 (00 00 00 a0 99 99 f1 3f)
ldc.r8 (00 00 00 a0 99 99 01 40)
ldc.r8 (00 00 00 60 66 66 0a 40)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Multiply'(float32, class [ScriptTypes]LindenLab.SecondLife.Vector)
ldc.r8 (00 00 00 a0 99 99 01 40)
ldc.r8 (00 00 00 a0 99 99 11 40)
ldc.r8 (00 00 00 60 66 66 1a 40)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureVectorEqual'(string, class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
ldarg.0
ldstr "(2 * <1.1, 2.2, 3.3>)"
ldc.i4 2
conv.r8
ldc.r8 (00 00 00 a0 99 99 f1 3f)
ldc.r8 (00 00 00 a0 99 99 01 40)
ldc.r8 (00 00 00 60 66 66 0a 40)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Multiply'(float32, class [ScriptTypes]LindenLab.SecondLife.Vector)
ldc.r8 (00 00 00 a0 99 99 f1 3f)
ldc.r8 (00 00 00 a0 99 99 01 40)
ldc.r8 (00 00 00 60 66 66 0a 40)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
ldc.i4 2
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Multiply'(class [ScriptTypes]LindenLab.SecondLife.Vector, float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureVectorEqual'(string, class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
ldarg.0
ldstr "(<2.2, 4.4, 6.6> * 2.0)"
ldc.r8 (00 00 00 00 00 00 00 40)
ldc.r8 (00 00 00 a0 99 99 01 40)
ldc.r8 (00 00 00 a0 99 99 11 40)
ldc.r8 (00 00 00 60 66 66 1a 40)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Multiply'(float32, class [ScriptTypes]LindenLab.SecondLife.Vector)
ldc.r8 (00 00 00 a0 99 99 11 40)
ldc.r8 (00 00 00 a0 99 99 21 40)
ldc.r8 (00 00 00 60 66 66 2a 40)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureVectorEqual'(string, class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
ldarg.0
ldstr "(2.0 * <2.2, 4.4, 6.6>)"
ldc.r8 (00 00 00 00 00 00 00 40)
ldc.r8 (00 00 00 a0 99 99 01 40)
ldc.r8 (00 00 00 a0 99 99 11 40)
ldc.r8 (00 00 00 60 66 66 1a 40)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Multiply'(float32, class [ScriptTypes]LindenLab.SecondLife.Vector)
ldc.r8 (00 00 00 a0 99 99 01 40)
ldc.r8 (00 00 00 a0 99 99 11 40)
ldc.r8 (00 00 00 60 66 66 1a 40)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
ldc.r8 (00 00 00 00 00 00 00 40)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Multiply'(class [ScriptTypes]LindenLab.SecondLife.Vector, float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureVectorEqual'(string, class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
ldarg.0
ldstr "<1,3,-5> * <4,-2,-1>"
ldc.i4 4
conv.r8
ldc.i4 2
neg
conv.r8
ldc.i4 1
neg
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
ldc.i4 1
conv.r8
ldc.i4 3
conv.r8
ldc.i4 5
neg
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call float32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Multiply'(class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
ldc.r8 (00 00 00 00 00 00 08 40)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureFloatEqual'(string, float32, float32)
ldarg.0
ldstr "<-1,0,0> * <0, 0, 0.707, 0.707>"
ldc.i4 0
conv.r8
ldc.i4 0
conv.r8
ldc.r8 (00 00 00 80 be 9f e6 3f)
ldc.r8 (00 00 00 80 be 9f e6 3f)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
ldc.i4 1
neg
conv.r8
ldc.i4 0
conv.r8
ldc.i4 0
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Multiply'(class [ScriptTypes]LindenLab.SecondLife.Quaternion, class [ScriptTypes]LindenLab.SecondLife.Vector)
ldc.i4 0
conv.r8
ldc.i4 1
neg
conv.r8
ldc.i4 0
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureVectorEqual'(string, class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
ldarg.0
ldstr "(<1.0, 2.0, 3.0, 4.0> * <5.0, 6.0, 7.0, 8.0>)"
ldc.r8 (00 00 00 00 00 00 14 40)
ldc.r8 (00 00 00 00 00 00 18 40)
ldc.r8 (00 00 00 00 00 00 1c 40)
ldc.r8 (00 00 00 00 00 00 20 40)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
ldc.r8 (00 00 00 00 00 00 f0 3f)
ldc.r8 (00 00 00 00 00 00 00 40)
ldc.r8 (00 00 00 00 00 00 08 40)
ldc.r8 (00 00 00 00 00 00 10 40)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Multiply'(class [ScriptTypes]LindenLab.SecondLife.Quaternion, class [ScriptTypes]LindenLab.SecondLife.Quaternion)
ldc.r8 (00 00 00 00 00 00 40 40)
ldc.r8 (00 00 00 00 00 00 40 40)
ldc.r8 (00 00 00 00 00 00 4c 40)
ldc.r8 (00 00 00 00 00 00 18 40)
neg
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureRotationEqual'(string, class [ScriptTypes]LindenLab.SecondLife.Quaternion, class [ScriptTypes]LindenLab.SecondLife.Quaternion)
ldarg.0
ldstr "(2 / 2)"
ldc.i4 2
ldc.i4 2
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Divide'(int32, int32)
ldc.i4 1
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(2.2 / 2)"
ldc.i4 2
conv.r8
ldc.r8 (00 00 00 a0 99 99 01 40)
call float64 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Divide'(float64, float64)
ldc.r8 (00 00 00 a0 99 99 f1 3f)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureFloatEqual'(string, float32, float32)
ldarg.0
ldstr "(3 / 1.5)"
ldc.r8 (00 00 00 00 00 00 f8 3f)
ldc.i4 3
conv.r8
call float64 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Divide'(float64, float64)
ldc.r8 (00 00 00 00 00 00 00 40)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureFloatEqual'(string, float32, float32)
ldarg.0
ldstr "(2.2 / 2.0)"
ldc.r8 (00 00 00 00 00 00 00 40)
ldc.r8 (00 00 00 a0 99 99 01 40)
call float64 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Divide'(float64, float64)
ldc.r8 (00 00 00 a0 99 99 f1 3f)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureFloatEqual'(string, float32, float32)
ldarg.0
ldstr "(<1.0, 2.0, 3.0> / 2)"
ldc.i4 2
conv.r8
ldc.r8 (00 00 00 00 00 00 f0 3f)
ldc.r8 (00 00 00 00 00 00 00 40)
ldc.r8 (00 00 00 00 00 00 08 40)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Divide'(float32, class [ScriptTypes]LindenLab.SecondLife.Vector)
ldc.r8 (00 00 00 00 00 00 e0 3f)
ldc.r8 (00 00 00 00 00 00 f0 3f)
ldc.r8 (00 00 00 00 00 00 f8 3f)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureVectorEqual'(string, class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
ldarg.0
ldstr "(<3.0, 6.0, 9.0> / 1.5)"
ldc.r8 (00 00 00 00 00 00 f8 3f)
ldc.r8 (00 00 00 00 00 00 08 40)
ldc.r8 (00 00 00 00 00 00 18 40)
ldc.r8 (00 00 00 00 00 00 22 40)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Divide'(float32, class [ScriptTypes]LindenLab.SecondLife.Vector)
ldc.r8 (00 00 00 00 00 00 00 40)
ldc.r8 (00 00 00 00 00 00 10 40)
ldc.r8 (00 00 00 00 00 00 18 40)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureVectorEqual'(string, class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
ldarg.0
ldstr "<-1,0,0> / <0, 0, 0.707, 0.707>"
ldc.i4 0
conv.r8
ldc.i4 0
conv.r8
ldc.r8 (00 00 00 80 be 9f e6 3f)
ldc.r8 (00 00 00 80 be 9f e6 3f)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
ldc.i4 1
neg
conv.r8
ldc.i4 0
conv.r8
ldc.i4 0
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Divide'(class [ScriptTypes]LindenLab.SecondLife.Quaternion, class [ScriptTypes]LindenLab.SecondLife.Vector)
ldc.i4 0
conv.r8
ldc.i4 1
conv.r8
ldc.i4 0
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureVectorEqual'(string, class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
ldarg.0
ldstr "(<1.0, 2.0, 3.0, 4.0> / <5.0, 6.0, 7.0, 8.0>)"
ldc.r8 (00 00 00 00 00 00 14 40)
ldc.r8 (00 00 00 00 00 00 18 40)
ldc.r8 (00 00 00 00 00 00 1c 40)
ldc.r8 (00 00 00 00 00 00 20 40)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
ldc.r8 (00 00 00 00 00 00 f0 3f)
ldc.r8 (00 00 00 00 00 00 00 40)
ldc.r8 (00 00 00 00 00 00 08 40)
ldc.r8 (00 00 00 00 00 00 10 40)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Divide'(class [ScriptTypes]LindenLab.SecondLife.Quaternion, class [ScriptTypes]LindenLab.SecondLife.Quaternion)
ldc.r8 (00 00 00 00 00 00 30 40)
neg
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 20 40)
neg
ldc.r8 (00 00 00 00 00 80 51 40)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureRotationEqual'(string, class [ScriptTypes]LindenLab.SecondLife.Quaternion, class [ScriptTypes]LindenLab.SecondLife.Quaternion)
ldarg.0
ldstr "(3 % 1)"
ldc.i4 1
ldc.i4 3
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Modulo'(int32, int32)
ldc.i4 0
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "(<1.0, 2.0, 3.0> % <4.0, 5.0, 6.0>)"
ldc.r8 (00 00 00 00 00 00 10 40)
ldc.r8 (00 00 00 00 00 00 14 40)
ldc.r8 (00 00 00 00 00 00 18 40)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
ldc.r8 (00 00 00 00 00 00 f0 3f)
ldc.r8 (00 00 00 00 00 00 00 40)
ldc.r8 (00 00 00 00 00 00 08 40)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Modulo'(class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
ldc.r8 (00 00 00 00 00 00 08 40)
neg
ldc.r8 (00 00 00 00 00 00 18 40)
ldc.r8 (00 00 00 00 00 00 08 40)
neg
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureVectorEqual'(string, class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
ldc.i4 1
stloc.s 3
ldarg.0
ldstr "i = 1;"
ldloc.s 3
ldc.i4 1
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldc.i4 1
dup
stloc.s 3
pop
ldc.i4 1
ldloc.s 3
add
dup
stloc.s 3
pop
ldarg.0
ldstr "i = 1; i += 1;"
ldloc.s 3
ldc.i4 2
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldc.i4 1
dup
stloc.s 3
pop
ldc.i4 1
ldloc.s 3
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Subtract'(int32, int32)
dup
stloc.s 3
pop
ldarg.0
ldstr "i = 1; i -= 1;"
ldloc.s 3
ldc.i4 0
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldc.i4 2
dup
stloc.s 3
pop
ldc.i4 2
ldloc.s 3
mul
dup
stloc.s 3
pop
ldarg.0
ldstr "i = 2; i *= 2;"
ldloc.s 3
ldc.i4 4
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldc.i4 1
dup
stloc.s 3
pop
ldc.r8 (00 00 00 00 00 00 e0 3f)
ldloc.s 3
conv.r8
mul
call int32 class [LslLibrary]LindenLab.SecondLife.LslRunTime::ToInteger(float32)
dup
stloc.s 3
pop
ldarg.0
ldstr "i = 1; i *= 0.5;"
ldloc.s 3
ldc.i4 0
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldc.i4 2
dup
stloc.s 3
pop
ldc.i4 2
ldloc.s 3
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Divide'(int32, int32)
dup
stloc.s 3
pop
ldarg.0
ldstr "i = 2; i /= 2;"
ldloc.s 3
ldc.i4 1
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldc.i4 1
neg
ldc.i4 -2147483648
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Divide'(int32, int32)
dup
stloc.s 3
pop
ldarg.0
ldstr "i = 0x80000000 / -1;"
ldloc.s 3
ldc.i4 -2147483648
neg
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldc.i4 3
dup
stloc.s 3
pop
ldc.i4 1
ldloc.s 3
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Modulo'(int32, int32)
dup
stloc.s 3
pop
ldarg.0
ldstr "i = 3; i %= 1;"
ldloc.s 3
ldc.i4 0
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldc.i4 1
neg
ldc.i4 -2147483648
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Modulo'(int32, int32)
dup
stloc.s 3
pop
ldarg.0
ldstr "i = 0x80000000 & -1;"
ldloc.s 3
ldc.i4 0
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldc.i4 1
dup
stloc.s 3
pop
ldarg.0
ldstr "postinc"
ldc.i4 1
ldloc.s 3
ldloc.s 3
ldc.i4.1
add
dup
stloc.s 3
pop
ceq
ldc.i4.0
ceq
ldc.i4 2
ldloc.s 3
ceq
ldc.i4.0
ceq
or
ldc.i4.0
ceq
ldc.i4 1
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldc.i4 1
dup
stloc.s 3
pop
ldarg.0
ldstr "preinc"
ldc.i4 2
ldloc.s 3
ldc.i4.1
add
dup
stloc.s 3
ceq
ldc.i4.0
ceq
ldc.i4 2
ldloc.s 3
ceq
ldc.i4.0
ceq
or
ldc.i4.0
ceq
ldc.i4 1
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldc.i4 2
dup
stloc.s 3
pop
ldarg.0
ldstr "postdec"
ldc.i4 2
ldloc.s 3
ldloc.s 3
ldc.i4.1
sub
dup
stloc.s 3
pop
ceq
ldc.i4.0
ceq
ldc.i4 1
ldloc.s 3
ceq
ldc.i4.0
ceq
or
ldc.i4.0
ceq
ldc.i4 1
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldc.i4 2
dup
stloc.s 3
pop
ldarg.0
ldstr "predec1"
ldc.i4 1
ldloc.s 3
ldc.i4.1
sub
dup
stloc.s 3
ceq
ldc.i4.0
ceq
ldc.i4 1
ldloc.s 3
ceq
ldc.i4.0
ceq
or
ldc.i4.0
ceq
ldc.i4 1
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldc.i4 2
dup
stloc.s 3
pop
ldloc.s 3
ldc.i4.1
sub
dup
stloc.s 3
pop
ldarg.0
ldstr "predec2"
ldloc.s 3
ldc.i4 1
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "((float)2)"
ldc.i4 2
conv.r8
ldc.r8 (00 00 00 00 00 00 00 40)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureFloatEqual'(string, float32, float32)
ldarg.0
ldstr "((string)2)"
ldc.i4 2
call string class [mscorlib]System.Convert::ToString(int32)
ldstr "2"
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureStringEqual'(string, string, string)
ldarg.0
ldstr "((integer) 1.5)"
ldc.r8 (00 00 00 00 00 00 f8 3f)
call int32 class [LslLibrary]LindenLab.SecondLife.LslRunTime::ToInteger(float32)
ldc.i4 1
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "((string) 1.5)"
ldc.r8 (00 00 00 00 00 00 f8 3f)
call string class [LslLibrary]LindenLab.SecondLife.LslRunTime::'ToString'(float32)
ldstr "1.500000"
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureStringEqual'(string, string, string)
ldarg.0
ldstr "((integer) \"0xF\")"
ldstr "0xF"
call int32 class [LslLibrary]LindenLab.SecondLife.LslRunTime::StringToInt(string)
ldc.i4 15
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "((integer) \"2\")"
ldstr "2"
call int32 class [LslLibrary]LindenLab.SecondLife.LslRunTime::StringToInt(string)
ldc.i4 2
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "((float) \"1.5\")"
ldstr "1.5"
call float32 class [LslLibrary]LindenLab.SecondLife.LslRunTime::StringToFloat(string)
ldc.r8 (00 00 00 00 00 00 f8 3f)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureFloatEqual'(string, float32, float32)
ldarg.0
ldstr "((vector) \"<1,2,3>\")"
ldstr "<1,2,3>"
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ParseVector'(string)
ldc.i4 1
conv.r8
ldc.i4 2
conv.r8
ldc.i4 3
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureVectorEqual'(string, class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
ldarg.0
ldstr "((quaternion) \"<1,2,3,4>\")"
ldstr "<1,2,3,4>"
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ParseQuaternion'(string)
ldc.i4 1
conv.r8
ldc.i4 2
conv.r8
ldc.i4 3
conv.r8
ldc.i4 4
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureRotationEqual'(string, class [ScriptTypes]LindenLab.SecondLife.Quaternion, class [ScriptTypes]LindenLab.SecondLife.Quaternion)
ldarg.0
ldstr "((string) <1,2,3>)"
ldc.i4 1
conv.r8
ldc.i4 2
conv.r8
ldc.i4 3
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ToString'(valuetype [ScriptTypes]LindenLab.SecondLife.Vector)
ldstr "<1.00000, 2.00000, 3.00000>"
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureStringEqual'(string, string, string)
ldarg.0
ldstr "((string) <1,2,3,4>)"
ldc.i4 1
conv.r8
ldc.i4 2
conv.r8
ldc.i4 3
conv.r8
ldc.i4 4
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ToString'(valuetype [ScriptTypes]LindenLab.SecondLife.Quaternion)
ldstr "<1.00000, 2.00000, 3.00000, 4.00000>"
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureStringEqual'(string, string, string)
ldarg.0
ldstr "((string) [1,2.5,<1,2,3>])"
ldc.i4 1
box [mscorlib]System.Int32
ldc.r8 (00 00 00 00 00 00 04 40)
box [mscorlib]System.Single
ldc.i4 1
conv.r8
ldc.i4 2
conv.r8
ldc.i4 3
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
box [ScriptTypes]LindenLab.SecondLife.Vector
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call string class [LslLibrary]LindenLab.SecondLife.LslRunTime::ListToString(class [mscorlib]System.Collections.ArrayList)
ldstr "12.500000<1.000000, 2.000000, 3.000000>"
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureStringEqual'(string, string, string)
ldc.i4 0
dup
stloc.s 3
pop
LabelTempJump37:
ldc.i4 10
ldloc.s 3
cgt
brfalse LabelTempJump38
ldloc.s 3
ldc.i4.1
add
dup
stloc.s 3
pop
br LabelTempJump37
LabelTempJump38:
ldarg.0
ldstr "i = 0; while(i < 10) ++i"
ldloc.s 3
ldc.i4 10
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldc.i4 0
dup
stloc.s 3
pop
LabelTempJump39:
ldloc.s 3
ldc.i4.1
add
dup
stloc.s 3
pop
ldc.i4 10
ldloc.s 3
cgt
brtrue LabelTempJump39
ldarg.0
ldstr "i = 0; do {++i;} while(i < 10);"
ldloc.s 3
ldc.i4 10
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldc.i4 0
dup
stloc.s 3
pop
LabelTempJump40:
ldc.i4 10
ldloc.s 3
cgt
brfalse LabelTempJump41
ldloc.s 3
ldc.i4.1
add
dup
stloc.s 3
pop
br LabelTempJump40
LabelTempJump41:
ldarg.0
ldstr "for(i = 0; i < 10; ++i);"
ldloc.s 3
ldc.i4 10
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldc.i4 1
dup
stloc.s 3
pop
br 'ulSkipAssign'
ldstr "Error"
ldc.i4 1
conv.r8
ldc.i4 0
conv.r8
ldc.i4 0
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
ldc.i4 1
conv.r8
call void class [LslLibrary]LindenLab.SecondLife.Library::'llSetText'(string, class [ScriptTypes]LindenLab.SecondLife.Vector, float32)
ldc.i4 2
dup
stloc.s 3
pop
'ulSkipAssign':
ldarg.0
ldstr "assignjump"
ldloc.s 3
ldc.i4 1
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "testReturn()"
ldarg.0
call instance int32 class LSL_00000000_0000_0000_0000_000000000000::'gtestReturn'()
ldc.i4 1
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "testReturnFloat()"
ldarg.0
call instance float32 class LSL_00000000_0000_0000_0000_000000000000::'gtestReturnFloat'()
ldc.r8 (00 00 00 00 00 00 f0 3f)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureFloatEqual'(string, float32, float32)
ldarg.0
ldstr "testReturnString()"
ldarg.0
call instance string class LSL_00000000_0000_0000_0000_000000000000::'gtestReturnString'()
ldstr "Test string"
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureStringEqual'(string, string, string)
ldarg.0
ldstr "testReturnVector()"
ldarg.0
call instance class [ScriptTypes]LindenLab.SecondLife.Vector class LSL_00000000_0000_0000_0000_000000000000::'gtestReturnVector'()
ldc.i4 1
conv.r8
ldc.i4 2
conv.r8
ldc.i4 3
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureVectorEqual'(string, class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
ldarg.0
ldstr "testReturnRotation()"
ldarg.0
call instance class [ScriptTypes]LindenLab.SecondLife.Quaternion class LSL_00000000_0000_0000_0000_000000000000::'gtestReturnRotation'()
ldc.i4 1
conv.r8
ldc.i4 2
conv.r8
ldc.i4 3
conv.r8
ldc.i4 4
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureRotationEqual'(string, class [ScriptTypes]LindenLab.SecondLife.Quaternion, class [ScriptTypes]LindenLab.SecondLife.Quaternion)
ldarg.0
ldstr "testReturnList()"
ldarg.0
call instance class [mscorlib]System.Collections.ArrayList class LSL_00000000_0000_0000_0000_000000000000::'gtestReturnList'()
ldc.i4 1
box [mscorlib]System.Int32
ldc.i4 2
box [mscorlib]System.Int32
ldc.i4 3
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureListEqual'(string, class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
ldarg.0
ldstr "testReturnVectorNested()"
ldarg.0
call instance class [ScriptTypes]LindenLab.SecondLife.Vector class LSL_00000000_0000_0000_0000_000000000000::'gtestReturnVectorNested'()
ldc.i4 1
conv.r8
ldc.i4 2
conv.r8
ldc.i4 3
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureVectorEqual'(string, class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
ldarg.0
ldstr "libveccall"
ldarg.0
call instance class [ScriptTypes]LindenLab.SecondLife.Vector class LSL_00000000_0000_0000_0000_000000000000::'gtestReturnVectorWithLibraryCall'()
ldc.i4 1
conv.r8
ldc.i4 2
conv.r8
ldc.i4 3
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureVectorEqual'(string, class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
ldarg.0
ldstr "librotcall"
ldarg.0
call instance class [ScriptTypes]LindenLab.SecondLife.Quaternion class LSL_00000000_0000_0000_0000_000000000000::'gtestReturnRotationWithLibraryCall'()
ldc.i4 1
conv.r8
ldc.i4 2
conv.r8
ldc.i4 3
conv.r8
ldc.i4 4
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureRotationEqual'(string, class [ScriptTypes]LindenLab.SecondLife.Quaternion, class [ScriptTypes]LindenLab.SecondLife.Quaternion)
ldarg.0
ldstr "testParameters(1)"
ldarg.0
ldc.i4 1
call instance int32 class LSL_00000000_0000_0000_0000_000000000000::'gtestParameters'(int32)
ldc.i4 2
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldc.i4 1
dup
stloc.s 3
pop
ldarg.0
ldstr "i = 1; testParameters(i)"
ldarg.0
ldloc.s 3
call instance int32 class LSL_00000000_0000_0000_0000_000000000000::'gtestParameters'(int32)
ldc.i4 2
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "testRecursion(10)"
ldarg.0
ldc.i4 10
call instance int32 class LSL_00000000_0000_0000_0000_000000000000::'gtestRecursion'(int32)
ldc.i4 0
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "gInteger"
ldarg.0
ldfld int32 LSL_00000000_0000_0000_0000_000000000000::'gInteger'
ldc.i4 5
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "gFloat"
ldarg.0
ldfld float32 LSL_00000000_0000_0000_0000_000000000000::'gFloat'
ldc.r8 (00 00 00 00 00 00 f8 3f)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureFloatEqual'(string, float32, float32)
ldarg.0
ldstr "gString"
ldarg.0
ldfld string LSL_00000000_0000_0000_0000_000000000000::'gString'
ldstr "foo"
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureStringEqual'(string, string, string)
ldarg.0
ldstr "gVector"
ldarg.0
ldfld class [ScriptTypes]LindenLab.SecondLife.Vector LSL_00000000_0000_0000_0000_000000000000::'gVector'
ldc.i4 1
conv.r8
ldc.i4 2
conv.r8
ldc.i4 3
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureVectorEqual'(string, class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
ldarg.0
ldstr "gRot"
ldarg.0
ldfld class [ScriptTypes]LindenLab.SecondLife.Quaternion LSL_00000000_0000_0000_0000_000000000000::'gRot'
ldc.i4 1
conv.r8
ldc.i4 2
conv.r8
ldc.i4 3
conv.r8
ldc.i4 4
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureRotationEqual'(string, class [ScriptTypes]LindenLab.SecondLife.Quaternion, class [ScriptTypes]LindenLab.SecondLife.Quaternion)
ldarg.0
ldstr "gList"
ldarg.0
ldfld class [mscorlib]System.Collections.ArrayList LSL_00000000_0000_0000_0000_000000000000::'gList'
ldc.i4 1
box [mscorlib]System.Int32
ldc.i4 2
box [mscorlib]System.Int32
ldc.i4 3
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureListEqual'(string, class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
ldarg.0
ldc.i4 1
stfld int32 LSL_00000000_0000_0000_0000_000000000000::'gInteger'
ldarg.0
ldfld int32 LSL_00000000_0000_0000_0000_000000000000::'gInteger'
pop
ldarg.0
ldstr "gInteger = 1"
ldarg.0
ldfld int32 LSL_00000000_0000_0000_0000_000000000000::'gInteger'
ldc.i4 1
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldc.r8 (00 00 00 00 00 00 e0 3f)
stfld float32 LSL_00000000_0000_0000_0000_000000000000::'gFloat'
ldarg.0
ldfld float32 LSL_00000000_0000_0000_0000_000000000000::'gFloat'
pop
ldarg.0
ldstr "gFloat = 0.5"
ldarg.0
ldfld float32 LSL_00000000_0000_0000_0000_000000000000::'gFloat'
ldc.r8 (00 00 00 00 00 00 e0 3f)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureFloatEqual'(string, float32, float32)
ldarg.0
ldstr "bar"
stfld string LSL_00000000_0000_0000_0000_000000000000::'gString'
ldarg.0
ldfld string LSL_00000000_0000_0000_0000_000000000000::'gString'
pop
ldarg.0
ldstr "gString = \"bar\""
ldarg.0
ldfld string LSL_00000000_0000_0000_0000_000000000000::'gString'
ldstr "bar"
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureStringEqual'(string, string, string)
ldarg.0
ldc.i4 3
conv.r8
ldc.i4 3
conv.r8
ldc.i4 3
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
stfld class [ScriptTypes]LindenLab.SecondLife.Vector LSL_00000000_0000_0000_0000_000000000000::'gVector'
ldarg.0
ldfld class [ScriptTypes]LindenLab.SecondLife.Vector LSL_00000000_0000_0000_0000_000000000000::'gVector'
pop
ldarg.0
ldstr "gVector = <3,3,3>"
ldarg.0
ldfld class [ScriptTypes]LindenLab.SecondLife.Vector LSL_00000000_0000_0000_0000_000000000000::'gVector'
ldc.i4 3
conv.r8
ldc.i4 3
conv.r8
ldc.i4 3
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureVectorEqual'(string, class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
ldarg.0
ldc.i4 3
conv.r8
ldc.i4 3
conv.r8
ldc.i4 3
conv.r8
ldc.i4 3
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
stfld class [ScriptTypes]LindenLab.SecondLife.Quaternion LSL_00000000_0000_0000_0000_000000000000::'gRot'
ldarg.0
ldfld class [ScriptTypes]LindenLab.SecondLife.Quaternion LSL_00000000_0000_0000_0000_000000000000::'gRot'
pop
ldarg.0
ldstr "gRot = <3,3,3,3>"
ldarg.0
ldfld class [ScriptTypes]LindenLab.SecondLife.Quaternion LSL_00000000_0000_0000_0000_000000000000::'gRot'
ldc.i4 3
conv.r8
ldc.i4 3
conv.r8
ldc.i4 3
conv.r8
ldc.i4 3
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureRotationEqual'(string, class [ScriptTypes]LindenLab.SecondLife.Quaternion, class [ScriptTypes]LindenLab.SecondLife.Quaternion)
ldarg.0
ldc.i4 4
box [mscorlib]System.Int32
ldc.i4 5
box [mscorlib]System.Int32
ldc.i4 6
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
stfld class [mscorlib]System.Collections.ArrayList LSL_00000000_0000_0000_0000_000000000000::'gList'
ldarg.0
ldfld class [mscorlib]System.Collections.ArrayList LSL_00000000_0000_0000_0000_000000000000::'gList'
pop
ldarg.0
ldstr "gList = [4,5,6]"
ldarg.0
ldfld class [mscorlib]System.Collections.ArrayList LSL_00000000_0000_0000_0000_000000000000::'gList'
ldc.i4 4
box [mscorlib]System.Int32
ldc.i4 5
box [mscorlib]System.Int32
ldc.i4 6
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureListEqual'(string, class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
ldarg.0
ldc.i4 3
conv.r8
ldc.i4 3
conv.r8
ldc.i4 3
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
stfld class [ScriptTypes]LindenLab.SecondLife.Vector LSL_00000000_0000_0000_0000_000000000000::'gVector'
ldarg.0
ldfld class [ScriptTypes]LindenLab.SecondLife.Vector LSL_00000000_0000_0000_0000_000000000000::'gVector'
pop
ldarg.0
ldstr "-gVector = <-3,-3,-3>"
ldarg.0
ldfld class [ScriptTypes]LindenLab.SecondLife.Vector LSL_00000000_0000_0000_0000_000000000000::'gVector'
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Negate'(class [ScriptTypes]LindenLab.SecondLife.Vector)
ldc.i4 3
neg
conv.r8
ldc.i4 3
neg
conv.r8
ldc.i4 3
neg
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureVectorEqual'(string, class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Vector)
ldarg.0
ldc.i4 3
conv.r8
ldc.i4 3
conv.r8
ldc.i4 3
conv.r8
ldc.i4 3
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
stfld class [ScriptTypes]LindenLab.SecondLife.Quaternion LSL_00000000_0000_0000_0000_000000000000::'gRot'
ldarg.0
ldfld class [ScriptTypes]LindenLab.SecondLife.Quaternion LSL_00000000_0000_0000_0000_000000000000::'gRot'
pop
ldarg.0
ldstr "-gRot = <-3,-3,-3,-3>"
ldarg.0
ldfld class [ScriptTypes]LindenLab.SecondLife.Quaternion LSL_00000000_0000_0000_0000_000000000000::'gRot'
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Negate'(class [ScriptTypes]LindenLab.SecondLife.Quaternion)
ldc.i4 3
neg
conv.r8
ldc.i4 3
neg
conv.r8
ldc.i4 3
neg
conv.r8
ldc.i4 3
neg
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureRotationEqual'(string, class [ScriptTypes]LindenLab.SecondLife.Quaternion, class [ScriptTypes]LindenLab.SecondLife.Quaternion)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
stloc.s 4
ldloca.s 4
ldc.i4 3
conv.r8
stfld float32 class [ScriptTypes]LindenLab.SecondLife.Vector::x
ldloca.s 4
ldfld float32 class [ScriptTypes]LindenLab.SecondLife.Vector::x
pop
ldarg.0
ldstr "v.x"
ldloca.s 4
ldfld float32 class [ScriptTypes]LindenLab.SecondLife.Vector::x
ldc.i4 3
conv.r8
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureFloatEqual'(string, float32, float32)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 f0 3f)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
stloc.s 5
ldloca.s 5
ldc.i4 5
conv.r8
stfld float32 class [ScriptTypes]LindenLab.SecondLife.Quaternion::s
ldloca.s 5
ldfld float32 class [ScriptTypes]LindenLab.SecondLife.Quaternion::s
pop
ldarg.0
ldstr "q.s"
ldloca.s 5
ldfld float32 class [ScriptTypes]LindenLab.SecondLife.Quaternion::s
ldc.i4 5
conv.r8
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureFloatEqual'(string, float32, float32)
ldarg.0
ldflda class [ScriptTypes]LindenLab.SecondLife.Vector LSL_00000000_0000_0000_0000_000000000000::'gVector'
ldc.r8 (00 00 00 00 00 80 31 40)
stfld float32 class [ScriptTypes]LindenLab.SecondLife.Vector::y
ldarg.0
ldflda class [ScriptTypes]LindenLab.SecondLife.Vector LSL_00000000_0000_0000_0000_000000000000::'gVector'
ldfld float32 class [ScriptTypes]LindenLab.SecondLife.Vector::y
pop
ldarg.0
ldstr "gVector.y = 17.5"
ldarg.0
ldflda class [ScriptTypes]LindenLab.SecondLife.Vector LSL_00000000_0000_0000_0000_000000000000::'gVector'
ldfld float32 class [ScriptTypes]LindenLab.SecondLife.Vector::y
ldc.r8 (00 00 00 00 00 80 31 40)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureFloatEqual'(string, float32, float32)
ldarg.0
ldflda class [ScriptTypes]LindenLab.SecondLife.Quaternion LSL_00000000_0000_0000_0000_000000000000::'gRot'
ldc.r8 (00 00 00 00 00 80 33 40)
stfld float32 class [ScriptTypes]LindenLab.SecondLife.Quaternion::z
ldarg.0
ldflda class [ScriptTypes]LindenLab.SecondLife.Quaternion LSL_00000000_0000_0000_0000_000000000000::'gRot'
ldfld float32 class [ScriptTypes]LindenLab.SecondLife.Quaternion::z
pop
ldarg.0
ldstr "gRot.z = 19.5"
ldarg.0
ldflda class [ScriptTypes]LindenLab.SecondLife.Quaternion LSL_00000000_0000_0000_0000_000000000000::'gRot'
ldfld float32 class [ScriptTypes]LindenLab.SecondLife.Quaternion::z
ldc.r8 (00 00 00 00 00 80 33 40)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureFloatEqual'(string, float32, float32)
ldc.i4 5
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList(object)
stloc.s 6
ldc.i4 5
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList(object)
stloc.s 7
ldarg.0
ldstr "leq1"
ldloc.s 6
ldloc.s 7
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureListEqual'(string, class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
ldarg.0
ldstr "leq2"
ldloc.s 6
ldc.i4 5
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureListEqual'(string, class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
ldarg.0
ldstr "leq3"
ldc.r8 (00 00 00 00 00 00 f8 3f)
box [mscorlib]System.Single
ldc.i4 6
box [mscorlib]System.Int32
ldc.i4 1
conv.r8
ldc.i4 2
conv.r8
ldc.i4 3
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
box [ScriptTypes]LindenLab.SecondLife.Vector
ldc.i4 1
conv.r8
ldc.i4 2
conv.r8
ldc.i4 3
conv.r8
ldc.i4 4
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
box [ScriptTypes]LindenLab.SecondLife.Quaternion
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
ldc.r8 (00 00 00 00 00 00 f8 3f)
box [mscorlib]System.Single
ldc.i4 6
box [mscorlib]System.Int32
ldc.i4 1
conv.r8
ldc.i4 2
conv.r8
ldc.i4 3
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
box [ScriptTypes]LindenLab.SecondLife.Vector
ldc.i4 1
conv.r8
ldc.i4 2
conv.r8
ldc.i4 3
conv.r8
ldc.i4 4
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
box [ScriptTypes]LindenLab.SecondLife.Quaternion
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureListEqual'(string, class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
ldarg.0
ldstr "sesc1"
ldstr "\\"
call int32 class [LslLibrary]LindenLab.SecondLife.Library::'llStringLength'(string)
ldc.i4 1
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "sesc2"
ldstr "    "
call int32 class [LslLibrary]LindenLab.SecondLife.Library::'llStringLength'(string)
ldc.i4 4
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "sesc3"
ldstr "\n"
call int32 class [LslLibrary]LindenLab.SecondLife.Library::'llStringLength'(string)
ldc.i4 1
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "sesc4"
ldstr "\""
call int32 class [LslLibrary]LindenLab.SecondLife.Library::'llStringLength'(string)
ldc.i4 1
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureIntegerEqual'(string, int32, int32)
ldarg.0
ldstr "testExpressionLists([testExpressionLists([]), \"bar\"]) == \"foofoobar\""
ldarg.0
ldarg.0
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call instance string class LSL_00000000_0000_0000_0000_000000000000::'gtestExpressionLists'(class [mscorlib]System.Collections.ArrayList)
ldstr "bar"
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call instance string class LSL_00000000_0000_0000_0000_000000000000::'gtestExpressionLists'(class [mscorlib]System.Collections.ArrayList)
ldstr "foofoobar"
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureStringEqual'(string, string, string)
ldc.i4 1
ldarg.0
ldc.i4 5
call instance int32 class LSL_00000000_0000_0000_0000_000000000000::'gcallOrderFunc'(int32)
ldarg.0
ldc.i4 4
call instance int32 class LSL_00000000_0000_0000_0000_000000000000::'gcallOrderFunc'(int32)
call int32 class [LslUserScript]LindenLab.SecondLife.LslUserScript::'Divide'(int32, int32)
ldarg.0
ldc.i4 3
call instance int32 class LSL_00000000_0000_0000_0000_000000000000::'gcallOrderFunc'(int32)
xor
ldarg.0
ldc.i4 2
call instance int32 class LSL_00000000_0000_0000_0000_000000000000::'gcallOrderFunc'(int32)
or
ldc.i4.0
ceq
ldarg.0
ldc.i4 1
call instance int32 class LSL_00000000_0000_0000_0000_000000000000::'gcallOrderFunc'(int32)
ldarg.0
ldc.i4 0
call instance int32 class LSL_00000000_0000_0000_0000_000000000000::'gcallOrderFunc'(int32)
mul
ldc.i4 1
or
ldc.i4.0
ceq
ldc.i4.0
ceq
ldc.i4.0
ceq
or
ldc.i4.0
ceq
or
ldc.i4.0
ceq
ldc.i4.0
ceq
brfalse LabelTempJump42
LabelTempJump42:
ldarg.0
ldstr "gCallOrder expected order"
ldarg.0
ldfld class [mscorlib]System.Collections.ArrayList LSL_00000000_0000_0000_0000_000000000000::'gCallOrder'
ldc.i4 5
box [mscorlib]System.Int32
ldc.i4 4
box [mscorlib]System.Int32
ldc.i4 3
box [mscorlib]System.Int32
ldc.i4 2
box [mscorlib]System.Int32
ldc.i4 1
box [mscorlib]System.Int32
ldc.i4 0
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gensureListEqual'(string, class [mscorlib]System.Collections.ArrayList, class [mscorlib]System.Collections.ArrayList)
ret
}
.method public hidebysig instance default void 'grunTests'() cil managed
{
.maxstack 500
ldarg.0
call instance void class LSL_00000000_0000_0000_0000_000000000000::'gtests'()
ldstr "All tests passed"
call void class [LslLibrary]LindenLab.SecondLife.Library::Print(string)
ldarg.0
ldc.i4 5
stfld int32 LSL_00000000_0000_0000_0000_000000000000::'gInteger'
ldarg.0
ldfld int32 LSL_00000000_0000_0000_0000_000000000000::'gInteger'
pop
ldarg.0
ldc.r8 (00 00 00 00 00 00 f8 3f)
stfld float32 LSL_00000000_0000_0000_0000_000000000000::'gFloat'
ldarg.0
ldfld float32 LSL_00000000_0000_0000_0000_000000000000::'gFloat'
pop
ldarg.0
ldstr "foo"
stfld string LSL_00000000_0000_0000_0000_000000000000::'gString'
ldarg.0
ldfld string LSL_00000000_0000_0000_0000_000000000000::'gString'
pop
ldarg.0
ldc.i4 1
conv.r8
ldc.i4 2
conv.r8
ldc.i4 3
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
stfld class [ScriptTypes]LindenLab.SecondLife.Vector LSL_00000000_0000_0000_0000_000000000000::'gVector'
ldarg.0
ldfld class [ScriptTypes]LindenLab.SecondLife.Vector LSL_00000000_0000_0000_0000_000000000000::'gVector'
pop
ldarg.0
ldc.i4 1
conv.r8
ldc.i4 2
conv.r8
ldc.i4 3
conv.r8
ldc.i4 4
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
stfld class [ScriptTypes]LindenLab.SecondLife.Quaternion LSL_00000000_0000_0000_0000_000000000000::'gRot'
ldarg.0
ldfld class [ScriptTypes]LindenLab.SecondLife.Quaternion LSL_00000000_0000_0000_0000_000000000000::'gRot'
pop
ldarg.0
ldc.i4 1
box [mscorlib]System.Int32
ldc.i4 2
box [mscorlib]System.Int32
ldc.i4 3
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
stfld class [mscorlib]System.Collections.ArrayList LSL_00000000_0000_0000_0000_000000000000::'gList'
ldarg.0
ldfld class [mscorlib]System.Collections.ArrayList LSL_00000000_0000_0000_0000_000000000000::'gList'
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
ldarg.0
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
stfld class [mscorlib]System.Collections.ArrayList LSL_00000000_0000_0000_0000_000000000000::'gCallOrder'
ldarg.0
ldfld class [mscorlib]System.Collections.ArrayList LSL_00000000_0000_0000_0000_000000000000::'gCallOrder'
pop
ret
}
.method public hidebysig instance default void edefaultstate_entry() cil managed
{
.maxstack 500
ldarg.0
call instance void class LSL_00000000_0000_0000_0000_000000000000::'grunTests'()
ret
}
}
