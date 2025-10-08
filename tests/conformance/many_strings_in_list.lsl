
list t1; list t2; list t3;
default
{
    state_entry()
    {
        // simply for speed under mono, appending to bigger lists gets slower
        integer i;
        for(i = 0; i < 20; ++i) {
            t1 += i;
            t2 += i;
            t3 += i;
        }
        llSleep(0.01);
        llOwnerSay("memory used: " + (string)llGetUsedMemory()); // would be wrong under Luau anyway
        llGetFreeMemory();
    }
}
