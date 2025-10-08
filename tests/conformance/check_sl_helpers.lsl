default {
    state_entry() {
        print(2);
        print("str_as_key");
        print((key)"key_as_key");
        print(<1,2,3,4>);
        print((key)"foo" == (key)"foo");
        print((key)"foo" != (key)"foo");
        print(<1,2,3> == <1,2,3>);
        print(<1,2,3> != <1,2,3>);
        print(<1,2,3,4> == <1,2,3,4>);
        print(<1,2,3,4> != <1,2,3,4>);
        print((key)"00000000-0000-0000-0000-000000000001");
        print((string)((key)"00000000-0000-0000-0000-000000000001"));
    }
}
