checkTruth(string name, integer val)
{
    if (!val)
    {
        llOwnerSay(name + ": FAILED");
        // Force a crash.
        llOwnerSay((string)(0/0));
    }
}

default
{
    state_entry()
    {
        string upper = "Ü Ä Ö";
        string lower = "ü ä ö";
        checkTruth("ToUpper", llToUpper(lower) == upper);
        checkTruth("ToLower", llToLower(upper) == lower);
    }
}
