.assembly extern mscorlib {.ver 1:0:5000:0}
.assembly extern LslLibrary {.ver 0:1:0:0}
.assembly extern LslUserScript {.ver 0:1:0:0}
.assembly extern ScriptTypes {.ver 0:1:0:0}
.assembly 'LSL_00000000_0000_0000_0000_000000000000' {.ver 0:0:0:0}
.class public auto ansi serializable beforefieldinit LSL_00000000_0000_0000_0000_000000000000 extends class [LslUserScript]LindenLab.SecondLife.LslUserScript
{
.field public string 'str_1'
.field public string 'str_2'
.field public string 'str_3'
.field public string 'str_4'
.field public string 'str_5'
.field public string 'str_6'
.field public int32 'i'
.field public float32 'f'
.field public float32 'actually_f'
.field public class [ScriptTypes]LindenLab.SecondLife.Vector 'v'
.field public valuetype [ScriptTypes]LindenLab.SecondLife.Key 'k'
.field public class [ScriptTypes]LindenLab.SecondLife.Quaternion 'r'
.field public class [ScriptTypes]LindenLab.SecondLife.Quaternion 'r2'
.field public class [ScriptTypes]LindenLab.SecondLife.Quaternion 'r3'
.field public class [ScriptTypes]LindenLab.SecondLife.Quaternion 'r4'
.field public class [mscorlib]System.Collections.ArrayList 'l'
.method public hidebysig specialname rtspecialname instance default void .ctor () cil managed
{
.maxstack 500
ldarg.0
ldstr ""
stfld string LSL_00000000_0000_0000_0000_000000000000::'str_1'
ldarg.0
ldarg.0
ldfld string LSL_00000000_0000_0000_0000_000000000000::'str_1'
stfld string LSL_00000000_0000_0000_0000_000000000000::'str_2'
ldarg.0
ldarg.0
ldfld string LSL_00000000_0000_0000_0000_000000000000::'str_2'
stfld string LSL_00000000_0000_0000_0000_000000000000::'str_3'
ldarg.0
ldstr ""
stfld string LSL_00000000_0000_0000_0000_000000000000::'str_4'
ldarg.0
ldarg.0
ldfld string LSL_00000000_0000_0000_0000_000000000000::'str_4'
stfld string LSL_00000000_0000_0000_0000_000000000000::'str_5'
ldarg.0
ldarg.0
ldfld string LSL_00000000_0000_0000_0000_000000000000::'str_5'
stfld string LSL_00000000_0000_0000_0000_000000000000::'str_6'
ldarg.0
ldc.i4 1
stfld int32 LSL_00000000_0000_0000_0000_000000000000::'i'
ldarg.0
ldarg.0
ldfld int32 LSL_00000000_0000_0000_0000_000000000000::'i'
conv.r8
stfld float32 LSL_00000000_0000_0000_0000_000000000000::'f'
ldarg.0
ldc.r8 (00 00 00 00 00 00 f0 3f)
stfld float32 LSL_00000000_0000_0000_0000_000000000000::'actually_f'
ldarg.0
ldarg.0
ldfld float32 LSL_00000000_0000_0000_0000_000000000000::'f'
ldc.i4 2
conv.r8
ldc.i4 3
conv.r8
call class [ScriptTypes]LindenLab.SecondLife.Vector class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateVector'(float32, float32, float32)
stfld class [ScriptTypes]LindenLab.SecondLife.Vector LSL_00000000_0000_0000_0000_000000000000::'v'
ldarg.0
ldstr "00000000-0000-0000-0000-000000000000"
call valuetype [ScriptTypes]LindenLab.SecondLife.Key class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateKey'(string)
stfld valuetype [ScriptTypes]LindenLab.SecondLife.Key LSL_00000000_0000_0000_0000_000000000000::'k'
ldarg.0
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 f0 3f)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
stfld class [ScriptTypes]LindenLab.SecondLife.Quaternion LSL_00000000_0000_0000_0000_000000000000::'r'
ldarg.0
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 00 00)
ldc.r8 (00 00 00 00 00 00 f0 3f)
call class [ScriptTypes]LindenLab.SecondLife.Quaternion class [LslUserScript]LindenLab.SecondLife.LslUserScript::'CreateQuaternion'(float32, float32, float32, float32)
stfld class [ScriptTypes]LindenLab.SecondLife.Quaternion LSL_00000000_0000_0000_0000_000000000000::'r2'
ldarg.0
ldarg.0
ldfld class [ScriptTypes]LindenLab.SecondLife.Quaternion LSL_00000000_0000_0000_0000_000000000000::'r2'
stfld class [ScriptTypes]LindenLab.SecondLife.Quaternion LSL_00000000_0000_0000_0000_000000000000::'r3'
ldarg.0
ldarg.0
ldfld class [ScriptTypes]LindenLab.SecondLife.Quaternion LSL_00000000_0000_0000_0000_000000000000::'r'
stfld class [ScriptTypes]LindenLab.SecondLife.Quaternion LSL_00000000_0000_0000_0000_000000000000::'r4'
ldarg.0
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::CreateList()
ldarg.0
ldfld int32 LSL_00000000_0000_0000_0000_000000000000::'i'
box [mscorlib]System.Int32
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Append(class [mscorlib]System.Collections.ArrayList, object)
ldarg.0
ldfld float32 LSL_00000000_0000_0000_0000_000000000000::'f'
box [mscorlib]System.Single
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Append(class [mscorlib]System.Collections.ArrayList, object)
ldarg.0
ldfld float32 LSL_00000000_0000_0000_0000_000000000000::'actually_f'
box [mscorlib]System.Single
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Append(class [mscorlib]System.Collections.ArrayList, object)
ldarg.0
ldfld class [ScriptTypes]LindenLab.SecondLife.Vector LSL_00000000_0000_0000_0000_000000000000::'v'
box [ScriptTypes]LindenLab.SecondLife.Vector
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Append(class [mscorlib]System.Collections.ArrayList, object)
ldarg.0
ldfld string LSL_00000000_0000_0000_0000_000000000000::'str_1'
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Append(class [mscorlib]System.Collections.ArrayList, object)
ldarg.0
ldfld valuetype [ScriptTypes]LindenLab.SecondLife.Key LSL_00000000_0000_0000_0000_000000000000::'k'
box [ScriptTypes]LindenLab.SecondLife.Key
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Append(class [mscorlib]System.Collections.ArrayList, object)
ldarg.0
ldfld class [ScriptTypes]LindenLab.SecondLife.Quaternion LSL_00000000_0000_0000_0000_000000000000::'r'
box [ScriptTypes]LindenLab.SecondLife.Quaternion
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Append(class [mscorlib]System.Collections.ArrayList, object)
ldarg.0
ldfld class [ScriptTypes]LindenLab.SecondLife.Vector LSL_00000000_0000_0000_0000_000000000000::'v'
box [ScriptTypes]LindenLab.SecondLife.Vector
call class [mscorlib]System.Collections.ArrayList class [LslUserScript]LindenLab.SecondLife.LslUserScript::Append(class [mscorlib]System.Collections.ArrayList, object)
stfld class [mscorlib]System.Collections.ArrayList LSL_00000000_0000_0000_0000_000000000000::'l'
ldarg.0
call instance void class [LslUserScript]LindenLab.SecondLife.LslUserScript::.ctor()
ret
}
.method public hidebysig instance default void edefaultstate_entry() cil managed
{
.maxstack 500
ret
}
}
