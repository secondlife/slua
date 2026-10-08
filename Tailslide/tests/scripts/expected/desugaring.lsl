float foo()
{
    return (float)1;
}

default
{
    state_entry()
    {
        integer i;
        i = i + 1;
        i = i + 1;
        i++;
        vector v = <(float)2, (float)2, (float)2>;
        v.x = v.x + 2;
        v.x = v.x + 1.00000;
        v.x++;
        float f = (float)3;
        f = (float)3;
        f = f + 3;
        key k = (key)"foo";
        string s = (string)k;
        llGetOwnerKey((key)"bar");
        llOwnerSay((string)k);
        llFrand((float)4);
        llFrand((float)(i = i + 1));
        foo();
    }
}
