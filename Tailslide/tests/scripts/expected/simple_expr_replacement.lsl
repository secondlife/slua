default
{
    state_entry()
    {
        llOwnerSay((string)-6);
        llOwnerSay((string)(llGetTime() - llGetTime() - llGetTime() - llGetTime()));
        llOwnerSay("foo" + llGetDisplayName(llGetOwner()) + "baz");
    }
}
