key foo = "foobar";
list bar = [foo];
default
{
    state_entry()
    {
        list baz = (list)foo;
        list quux = [(key)"foobar", 2];
        print(foo);
        print(bar);
        print(baz);
        print(quux);
        if ((foo))
            ;
        while (foo)
            ;
        for (foo, foo; foo; foo)
            ;
        llGetOwnerKey("foobar");
    }
}
