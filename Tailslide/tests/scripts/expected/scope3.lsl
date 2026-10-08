list foo = ["quux", <0.00000, 0.00000, 3.00000>, 3.00000];
test()
{
}

default
{
    state_entry()
    {
        integer test = 1;
        test();
        test = 2;
    }

    touch_start(integer x)
    {
        jump test;
        test();
        @test;
        jump llOwnerSay;
        llOwnerSay(llDumpList2String(foo, "|"));
        llOwnerSay(llDumpList2String(["quux", <0.00000, 0.00000, 3.00000>, 3.00000], "|"));
        @llOwnerSay;
    }
}
