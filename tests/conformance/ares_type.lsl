assertTypesMatch(list input, list types) {
    integer len = (input != []);

    if (len != (types != [])) {
        print(1/0);
    }
    integer i;
    for(i=0; i<len; ++i) {
        if (llGetListEntryType(input, i) != llList2Integer(types, i)) {
            print("Mismatch at " + (string)i);
            print(1/0);
        }
    }
}

default {
    state_entry() {
        // Definition order is important to trigger the repro case
        // With keys and strings swapping type
        list f = [(key)"", "", "foobar", (key)"foobar"];

        // Force a yield so script state gets serialized
        print("yield");

        assertTypesMatch(f, [TYPE_KEY, TYPE_STRING, TYPE_STRING, TYPE_KEY]);
        print("Test succeeded");
    }
}
