default
{
    state_entry()
    {
        llOwnerSay((string)(whatever("2 + 2 * 2 + 2") - 2));
        llOwnerSay((string)(whatever("llGetTime() + llGetTime() + llGetTime()") - llGetTime()));
    }
}
