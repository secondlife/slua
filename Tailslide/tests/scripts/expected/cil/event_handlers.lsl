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
.method public hidebysig instance default void edefaultat_rot_target(int32 'tnum', class [ScriptTypes]LindenLab.SecondLife.Quaternion 'targetrot', class [ScriptTypes]LindenLab.SecondLife.Quaternion 'ourrot') cil managed
{
.maxstack 500
ret
}
.method public hidebysig instance default void edefaultat_target(int32 'tnum', class [ScriptTypes]LindenLab.SecondLife.Vector 'targetpos', class [ScriptTypes]LindenLab.SecondLife.Vector 'ourpos') cil managed
{
.maxstack 500
ret
}
.method public hidebysig instance default void edefaultattach(valuetype [ScriptTypes]LindenLab.SecondLife.Key 'id') cil managed
{
.maxstack 500
ret
}
.method public hidebysig instance default void edefaultchanged(int32 'change') cil managed
{
.maxstack 500
ret
}
.method public hidebysig instance default void edefaultcollision(int32 'num_detected') cil managed
{
.maxstack 500
ret
}
.method public hidebysig instance default void edefaultcollision_end(int32 'num_detected') cil managed
{
.maxstack 500
ret
}
.method public hidebysig instance default void edefaultcollision_start(int32 'num_detected') cil managed
{
.maxstack 500
ret
}
.method public hidebysig instance default void edefaultcontrol(valuetype [ScriptTypes]LindenLab.SecondLife.Key 'id', int32 'level', int32 'edge') cil managed
{
.maxstack 500
ret
}
.method public hidebysig instance default void edefaultdataserver(valuetype [ScriptTypes]LindenLab.SecondLife.Key 'queryid', string 'data') cil managed
{
.maxstack 500
ret
}
.method public hidebysig instance default void edefaultemail(string 'time', string 'address', string 'subj', string 'message', int32 'num_left') cil managed
{
.maxstack 500
ret
}
.method public hidebysig instance default void edefaulthttp_request(valuetype [ScriptTypes]LindenLab.SecondLife.Key 'id', string 'method', string 'body') cil managed
{
.maxstack 500
ret
}
.method public hidebysig instance default void edefaulthttp_response(valuetype [ScriptTypes]LindenLab.SecondLife.Key 'request_id', int32 'status', class [mscorlib]System.Collections.ArrayList 'metadata', string 'body') cil managed
{
.maxstack 500
ret
}
.method public hidebysig instance default void edefaultland_collision(class [ScriptTypes]LindenLab.SecondLife.Vector 'pos') cil managed
{
.maxstack 500
ret
}
.method public hidebysig instance default void edefaultland_collision_end(class [ScriptTypes]LindenLab.SecondLife.Vector 'pos') cil managed
{
.maxstack 500
ret
}
.method public hidebysig instance default void edefaultland_collision_start(class [ScriptTypes]LindenLab.SecondLife.Vector 'pos') cil managed
{
.maxstack 500
ret
}
.method public hidebysig instance default void edefaultlink_message(int32 'sender_num', int32 'num', string 'str', valuetype [ScriptTypes]LindenLab.SecondLife.Key 'id') cil managed
{
.maxstack 500
ret
}
.method public hidebysig instance default void edefaultchat(int32 'channel', string 'name', valuetype [ScriptTypes]LindenLab.SecondLife.Key 'id', string 'message') cil managed
{
.maxstack 500
ret
}
.method public hidebysig instance default void edefaultmoney(valuetype [ScriptTypes]LindenLab.SecondLife.Key 'id', int32 'amount') cil managed
{
.maxstack 500
ret
}
.method public hidebysig instance default void edefaultmoving_end() cil managed
{
.maxstack 500
ret
}
.method public hidebysig instance default void edefaultmoving_start() cil managed
{
.maxstack 500
ret
}
.method public hidebysig instance default void edefaultno_sensor() cil managed
{
.maxstack 500
ret
}
.method public hidebysig instance default void edefaultnot_at_rot_target() cil managed
{
.maxstack 500
ret
}
.method public hidebysig instance default void edefaultnot_at_target() cil managed
{
.maxstack 500
ret
}
.method public hidebysig instance default void edefaultobject_rez(valuetype [ScriptTypes]LindenLab.SecondLife.Key 'id') cil managed
{
.maxstack 500
ret
}
.method public hidebysig instance default void edefaultrez(int32 'start_param') cil managed
{
.maxstack 500
ret
}
.method public hidebysig instance default void edefaultremote_event(int32 'event_type', valuetype [ScriptTypes]LindenLab.SecondLife.Key 'channel', valuetype [ScriptTypes]LindenLab.SecondLife.Key 'message_id', string 'sender', int32 'idata', string 'sdata') cil managed
{
.maxstack 500
ret
}
.method public hidebysig instance default void edefaultrun_time_perms(int32 'perm') cil managed
{
.maxstack 500
ret
}
.method public hidebysig instance default void edefaultsensor(int32 'num_detected') cil managed
{
.maxstack 500
ret
}
.method public hidebysig instance default void edefaultstate_entry() cil managed
{
.maxstack 500
ret
}
.method public hidebysig instance default void edefaultstate_exit() cil managed
{
.maxstack 500
ret
}
.method public hidebysig instance default void edefaulttimer() cil managed
{
.maxstack 500
ret
}
.method public hidebysig instance default void edefaulttouch(int32 'num_detected') cil managed
{
.maxstack 500
ret
}
.method public hidebysig instance default void edefaulttouch_end(int32 'num_detected') cil managed
{
.maxstack 500
ret
}
.method public hidebysig instance default void edefaulttouch_start(int32 'num_detected') cil managed
{
.maxstack 500
ret
}
}
