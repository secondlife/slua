.assembly extern mscorlib {.ver 1:0:5000:0}
.assembly extern LslLibrary {.ver 0:1:0:0}
.assembly extern LslUserScript {.ver 0:1:0:0}
.assembly extern ScriptTypes {.ver 0:1:0:0}
.assembly 'LSL_00000000_0000_0000_0000_000000000000' {.ver 0:0:0:0}
.class public auto ansi serializable beforefieldinit LSL_00000000_0000_0000_0000_000000000000 extends class [LslUserScript]LindenLab.SecondLife.LslUserScript
{
.field public int32 'gi'
.field public float32 'gf'
.field public string 'gs'
.field public valuetype [ScriptTypes]LindenLab.SecondLife.Key 'gk'
.field public class [ScriptTypes]LindenLab.SecondLife.Vector 'gv'
.field public class [ScriptTypes]LindenLab.SecondLife.Quaternion 'gr'
.field public class [mscorlib]System.Collections.ArrayList 'gl'
.method public hidebysig specialname rtspecialname instance default void .ctor () cil managed
{
.maxstack 500
ldarg.0
ldc.i4.0
stfld int32 LSL_00000000_0000_0000_0000_000000000000::'gi'
ldarg.0
ldc.r8 (00 00 00 00 00 00 00 00)
stfld float32 LSL_00000000_0000_0000_0000_000000000000::'gf'
ldarg.0
ldstr ""
stfld string LSL_00000000_0000_0000_0000_000000000000::'gs'
ldarg.0
ldstr ""
call valuetype [ScriptTypes]LindenLab.SecondLife.Key class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateKey'(string)
stfld valuetype [ScriptTypes]LindenLab.SecondLife.Key LSL_00000000_0000_0000_0000_000000000000::'gk'
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
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
stfld class [mscorlib]System.Collections.ArrayList LSL_00000000_0000_0000_0000_000000000000::'gl'
ldarg.0
call instance void class [LslUserScript]LindenLab.SecondLife.LslUserScript::.ctor()
ret
}
.method public hidebysig instance default void edefaulttouch_start(int32 'num_detected') cil managed
{
.maxstack 500
.locals init (int32, float32, string, valuetype [ScriptTypes]LindenLab.SecondLife.Key, class [ScriptTypes]LindenLab.SecondLife.Vector, class [ScriptTypes]LindenLab.SecondLife.Quaternion, class [mscorlib]System.Collections.ArrayList)
ldc.i4.0
stloc.s 0
ldc.r8 (00 00 00 00 00 00 00 00)
stloc.s 1
ldstr ""
stloc.s 2
ldstr ""
call valuetype [ScriptTypes]LindenLab.SecondLife.Key class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateKey'(string)
stloc.s 3
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
stloc.s 4
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 f0 3f)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
stloc.s 5
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
stloc.s 6
ldarg.0
ldc.i4 1
stfld int32 LSL_00000000_0000_0000_0000_000000000000::'gi'
ldarg.0
ldfld int32 LSL_00000000_0000_0000_0000_000000000000::'gi'
dup
stloc.s 0
pop
ldarg.0
ldc.r8 (00 00 00 00 00 00 f0 3f)
stfld float32 LSL_00000000_0000_0000_0000_000000000000::'gf'
ldarg.0
ldfld float32 LSL_00000000_0000_0000_0000_000000000000::'gf'
dup
stloc.s 1
pop
ldarg.0
ldstr "foo"
stfld string LSL_00000000_0000_0000_0000_000000000000::'gs'
ldarg.0
ldfld string LSL_00000000_0000_0000_0000_000000000000::'gs'
dup
stloc.s 2
pop
ldarg.0
call valuetype [ScriptTypes]LindenLab.SecondLife.Key class [LslLibrary]LindenLab.SecondLife.Library::'llGetKey'()
stfld valuetype [ScriptTypes]LindenLab.SecondLife.Key LSL_00000000_0000_0000_0000_000000000000::'gk'
ldarg.0
ldfld valuetype [ScriptTypes]LindenLab.SecondLife.Key LSL_00000000_0000_0000_0000_000000000000::'gk'
dup
stloc.s 3
pop
ldarg.0
ldc.r8 (00 00 00 00 00 00 f0 3f)
ldc.r8 (00 00 00 00 00 00 f0 3f)
ldc.r8 (00 00 00 00 00 00 f0 3f)
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
stfld class [ScriptTypes]LindenLab.SecondLife.Vector LSL_00000000_0000_0000_0000_000000000000::'gv'
ldarg.0
ldfld class [ScriptTypes]LindenLab.SecondLife.Vector LSL_00000000_0000_0000_0000_000000000000::'gv'
dup
stloc.s 4
pop
ldarg.0
ldc.r8 (00 00 00 00 00 00 f0 3f)
ldc.r8 (00 00 00 00 00 00 f0 3f)
ldc.r8 (00 00 00 00 00 00 f0 3f)
ldc.r8 (00 00 00 00 00 00 f0 3f)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
stfld class [ScriptTypes]LindenLab.SecondLife.Quaternion LSL_00000000_0000_0000_0000_000000000000::'gr'
ldarg.0
ldfld class [ScriptTypes]LindenLab.SecondLife.Quaternion LSL_00000000_0000_0000_0000_000000000000::'gr'
dup
stloc.s 5
pop
ldarg.0
ldc.i4 1
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Prepend(object, class [mscorlib]System.Collections.ArrayList)
stfld class [mscorlib]System.Collections.ArrayList LSL_00000000_0000_0000_0000_000000000000::'gl'
ldarg.0
ldfld class [mscorlib]System.Collections.ArrayList LSL_00000000_0000_0000_0000_000000000000::'gl'
dup
stloc.s 6
pop
ldloca.s 4
ldarg.0
ldflda class [ScriptTypes]LindenLab.SecondLife.Vector LSL_00000000_0000_0000_0000_000000000000::'gv'
ldc.r8 (00 00 00 00 00 00 f0 3f)
stfld float32 class [ScriptTypes]LindenLab.SecondLife.Vector::x
ldarg.0
ldflda class [ScriptTypes]LindenLab.SecondLife.Vector LSL_00000000_0000_0000_0000_000000000000::'gv'
ldfld float32 class [ScriptTypes]LindenLab.SecondLife.Vector::x
stfld float32 class [ScriptTypes]LindenLab.SecondLife.Vector::x
ldloca.s 4
ldfld float32 class [ScriptTypes]LindenLab.SecondLife.Vector::x
pop
ldarg.0
ldarg.s 'num_detected'
stfld int32 LSL_00000000_0000_0000_0000_000000000000::'gi'
ldarg.0
ldfld int32 LSL_00000000_0000_0000_0000_000000000000::'gi'
dup
stloc.s 0
pop
ldc.i4 1
dup
starg.s 'num_detected'
pop
ldstr "foobar"
call valuetype [ScriptTypes]LindenLab.SecondLife.Key class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateKey'(string)
dup
stloc.s 3
pop
ldloc.s 3
call string class [LslUserScript]LindenLab.SecondLife.LslUserScript::'ToString'(valuetype [ScriptTypes]LindenLab.SecondLife.Key)
dup
stloc.s 2
pop
ret
}
}
