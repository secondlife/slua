float foo = -2.0e+999;
float bar = 2.0e+999;
default
{
    state_entry()
    {
        llSetPos(<-2.0e+999, 2.0e+999, 0>);
        llSetRot(<-2.0e+999, 2.0e+999, 0, 0>);
    }
}
