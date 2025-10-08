// Value we expect if double precision was used for temporaries or storage
// This is the max integral value a float32 can store without rounding
integer F32_MAX = 16777216;

checkTruth(string name, integer val)
{
    if (!val)
    {
        llOwnerSay(name + ": FAILED");
        // Force a crash.
        llOwnerSay((string)(0/0));
    }
}

key implicitKeyConversion(integer foo, string bar, key val)
{
    return val;
}

default
{
    state_entry()
    {
        checkTruth("integer->string", llList2String([2], 0) == "2");
        // This is not meant to roundtrip correctly. Mono has very janky precision when using the
        // F<n> format specifier, we need to replicate it.
        checkTruth("float->string", llList2String([(float)F32_MAX], 0) == "16777220.000000");
        checkTruth("vector->string", llList2String([<1, 2, 3>], 0) == "<1.000000, 2.000000, 3.000000>");
        checkTruth("rot->string", llList2String([<1, 2, 3, 4>], 0) == "<1.000000, 2.000000, 3.000000, 4.000000>");
        checkTruth("key->string", llList2String([(key)NULL_KEY], 0) == NULL_KEY);
        checkTruth("string->string", llList2String(["foo"], 0) == "foo");

        checkTruth("negative indices", llList2String(["1", "2", "3"], -2) == "2");
        checkTruth("out of bounds", llList2String(["1", "2", "3"], 3) == "");

        checkTruth("string->integer", llList2Integer(["123foo"], 0) == 123);
        checkTruth("integer->integer", llList2Integer([123], 0) == 123);
        checkTruth("key->integer", llList2Integer([(key)"123foo"], 0) == 0);

        checkTruth("integer->float", llList2Float([1], 0) == 1.0);
        checkTruth("string->float", llList2Float(["1.1"], 0) == 1.1);
        checkTruth("float->float", llList2Float([1.1], 0) == 1.1);
        checkTruth("other->float", llList2Float([(key)"1.1"], 0) == 0.0);

        checkTruth("string->key", llList2Key(["foo"], 0) == (key)"foo");
        checkTruth("key->key", llList2Key([(key)"foo"], 0) == (key)"foo");
        checkTruth("(implicit)key->key", llList2Key([implicitKeyConversion(0, "", "foo")], 0) == (key)"foo");
        // _not_ NULL_KEY!
        checkTruth("other->key", llList2Key([1.0], 0) == (key)"");

        checkTruth("vector->vector", llList2Vector([<1,2,3>], 0) == <1,2,3>);
        checkTruth("other->vector", llList2Vector(["<1,2,3>"], 0) == ZERO_VECTOR);

        checkTruth("rot->rot", llList2Rot([<1,2,3,4>], 0) == <1,2,3,4>);
        checkTruth("other->rot", llList2Rot(["<1,2,3,4>"], 0) == ZERO_ROTATION);

        checkTruth("list length", llGetListLength([1, 2, 3]) == 3);

        list l = [1, 1.1, "foo", (key)NULL_KEY, <1, 2, 3>, <1, 2, 3, 4>];

        checkTruth("string type", llGetListEntryType(l, 0) == TYPE_INTEGER);
        checkTruth("float type", llGetListEntryType(l, 1) == TYPE_FLOAT);
        checkTruth("string type", llGetListEntryType(l, 2) == TYPE_STRING);
        checkTruth("key type", llGetListEntryType(l, 3) == TYPE_KEY);
        checkTruth("vector type", llGetListEntryType(l, 4) == TYPE_VECTOR);
        checkTruth("rot type", llGetListEntryType(l, 5) == TYPE_ROTATION);
        checkTruth("invalid type", llGetListEntryType(l, 6) == TYPE_INVALID);

        list NUMBERS = [0,1,2,3,4,5,6,7,8,9];
        checkTruth("one elem", (string)llList2List(NUMBERS, 2, 2) == "2");
        checkTruth("multiple elems", (string)llList2List(NUMBERS, 2, 4) == "234");
        checkTruth("empty list", (string)llList2List([], 0, 0) == "");

        checkTruth("wraparound 1", (string)llList2List(NUMBERS, 8, 1) == "0189");
        checkTruth("wraparound 2", (string)llList2List(NUMBERS, 8,-9) == "0189");
        checkTruth("wraparound 3", (string)llList2List(NUMBERS,-2,-9) == "0189");
        checkTruth("wraparound 4", (string)llList2List(NUMBERS,-2, 1) == "0189");
        checkTruth("extra negative 1", (string)llList2List(NUMBERS,-200, 1) == "01");
        checkTruth("extra negative 2", (string)llList2List(NUMBERS,-200, -199) == "");
        checkTruth("huge indices", (string)llList2List(NUMBERS, 200, 201) == "");
        checkTruth("huge indices2", (string)llList2List(NUMBERS, 201, 200) == "0123456789");
        checkTruth("huge indices3", (string)llList2List(NUMBERS, 1, 200) == "123456789");
        checkTruth("llDumpList2String", llDumpList2String(l, "|") == "1|1.100000|foo|00000000-0000-0000-0000-000000000000|<1.000000, 2.000000, 3.000000>|<1.000000, 2.000000, 3.000000, 4.000000>");

        list nums = [1, 2, 3, 4, 5, 6];
        checkTruth("deletelist1", (string)llDeleteSubList(nums, 1, 2) == "1456");
        checkTruth("deletelist2", (string)llDeleteSubList(nums, 3, 1) == "3");

        checkTruth("insertlist1", (string)llListInsertList([1, 4], [2, 3], 1) == "1234");
        checkTruth("insertlist2", (string)llListInsertList([1, 2], [3, 4], 2) == "1234");
        checkTruth("insertlist3", (string)llListInsertList([], [1, 2, 3, 4], 2) == "1234");
        checkTruth("insertlist4", (string)llListInsertList([1, 2, 3, 4], [], 2) == "1234");

        checkTruth("replacelist1", (string)llListReplaceList(nums, [7, 7], 1, 2) == "177456");
        checkTruth("replacelist2", (string)llListReplaceList(nums, [9, 9], 3, 1) == "399");

        // adapted from https://wiki.secondlife.com/wiki/LlListReplaceList_Test
        list foo = ["1", "2", "3", "4"];
        list bar = ["a", "b"];
        checkTruth("replacelisttest1", (string)llListReplaceList(foo,bar,0,0) == "ab234");
        checkTruth("replacelisttest2", (string)llListReplaceList(foo,bar,-1,-1) == "123ab");
        checkTruth("replacelisttest3", (string)llListReplaceList(foo,bar,0,-1) == "ab");
        checkTruth("replacelisttest4", (string)llListReplaceList(foo,bar,6,6) == "1234ab");
        checkTruth("replacelisttest5", (string)llListReplaceList(foo,bar,1,2) == "1ab4");
        checkTruth("replacelisttest6", (string)llListReplaceList(foo,bar,2,1) == "ab");
        checkTruth("replacelisttest7", (string)llListReplaceList(foo,bar,3,1) == "3ab");
    }
}
