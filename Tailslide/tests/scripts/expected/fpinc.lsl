default
{
    state_entry()
    {
        float x;
        x++;
        x--;
        vector v;
        llOwnerSay((string)(v.x++));
        print((string)(++v.x));
    }
}
