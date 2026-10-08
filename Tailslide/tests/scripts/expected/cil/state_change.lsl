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
ldarg.0
ldstr "foo"
call instance void class [LslUserScript]LindenLab.SecondLife.LslUserScript::ChangeState(string)
ret
ret
}
.method public hidebysig instance default void efoostate_entry() cil managed
{
.maxstack 500
ldarg.0
ldstr "bar"
call instance void class [LslUserScript]LindenLab.SecondLife.LslUserScript::ChangeState(string)
ret
ret
}
.method public hidebysig instance default void ebarstate_entry() cil managed
{
.maxstack 500
ldarg.0
ldstr "default"
call instance void class [LslUserScript]LindenLab.SecondLife.LslUserScript::ChangeState(string)
ret
ret
}
}
