integer cur_emailer = 0;
integer max_emailer;
list tlmlPages;
list pages;
list indexes;
list offsets;
list sizes;
integer page;
string name;
integer line;
key query;
integer handle;
list TightListParse(string a)
{
    string b = llGetSubString(a, 0, 0);
    return llParseStringKeepNulls(llDeleteSubString(a, 0, 0), [b], []);
}

string TightListDump(list a, string b)
{
    string c = (string)a;
    if (llStringLength(b) == 1)
        if (llSubStringIndex(c, b) == -1)
            jump end;
    integer d = -llStringLength(b += "|\\/?!@#$%^&*()_=:;~{}[],\n\" qQxXzZ");
    while (1 + llSubStringIndex(c, llGetSubString(b, d, d)) && d)
        ++d;
    b = llGetSubString(b, d, d);
    @end;
    c = "";
    return b + llDumpList2String(a, b);
}

sendEmail(string a, string s, string m)
{
    if (max_emailer > 0)
    {
        llMessageLinked(LINK_THIS, cur_emailer, TightListDump([a + "@lsl.secondlife.com", s, m], "!"), "");
        cur_emailer = (cur_emailer + 1) % max_emailer;
    }
    else
        llEmail(a + "@lsl.secondlife.com", s, m);
}

default
{
    state_entry()
    {
        llOwnerSay("Caching pages...");
        pages = [];
        indexes = [];
        offsets = [];
        sizes = [];
        page = 0;
        line = 0;
        if (llGetInventoryNumber(INVENTORY_NOTECARD) > 0)
        {
            name = llGetInventoryName(INVENTORY_NOTECARD, 0);
            llOwnerSay("Caching page " + name);
            query = llGetNotecardLine(name, line);
        }
        else
            llOwnerSay("No page found.");
    }

    touch_start(integer c)
    {
        if (llDetectedKey(0) != llGetOwner())
            return;
        llResetScript();
    }

    dataserver(key id, string data)
    {
        if (query == id)
        {
            if (data != "\n\n\n")
            {
                if (line == 0 && llGetSubString(data, -6, -1) == "TLML-L")
                {
                    data = "\n\n\n" + "\n\n\n";
                }
                else
                {
                    pages += [data];
                    line = line + 1;
                    query = llGetNotecardLine(name, line);
                }
            }
            if (llSubStringIndex(data, "\n\n\n") + 1)
            {
                if (data == "\n\n\n" + "\n\n\n")
                    tlmlPages += [name];
                else
                {
                    indexes += [name];
                    sizes += [line];
                    offsets += [llGetListLength(pages) - line];
                }
                page = page + 1;
                if (page < llGetInventoryNumber(INVENTORY_NOTECARD))
                {
                    line = 0;
                    name = llGetInventoryName(INVENTORY_NOTECARD, page);
                    llOwnerSay("Caching page " + name);
                    query = llGetNotecardLine(name, line);
                }
                else
                {
                    llOwnerSay("Caching ended.");
                    state ready;
                }
            }
        }
    }

    changed(integer c)
    {
        if (c & CHANGED_INVENTORY)
            llResetScript();
    }
}
state ready
{
    state_entry()
    {
        max_emailer = llGetInventoryNumber(INVENTORY_SCRIPT) - 1;
        handle = llListen(777, "", "", "");
        llOwnerSay("Server ready on channel " + (string)777);
        llSetTimerEvent(5.00000);
        llOwnerSay((string)llGetKey() + "@lsl.secondlife.com");
        if (llGetObjectDesc() == "")
            llSetObjectDesc("Server");
    }

    changed(integer c)
    {
        if (c & CHANGED_INVENTORY)
            llResetScript();
    }

    timer()
    {
        llSay(-9, "U" + TightListDump([llList2String(indexes + tlmlPages, 0), 0, 777, llGetObjectDesc()], "|"));
        llGetNextEmail("", "");
    }

    listen(integer ch, string n, key id, string msg)
    {
        string a = llGetSubString(msg, 0, 0);
        if (a != "U")
            return;
        list info = TightListParse(llDeleteSubString(msg, 0, 0));
        if (llGetListLength(info) < 3)
            return;
        string req = llList2String(info, 0);
        integer i = llListFindList(indexes, [req]);
        string m = llList2String(info, 1);
        integer c = (integer)llList2String(info, 2);
        if (m != "0")
            return;
        if (i < 0)
        {
            i = llListFindList(tlmlPages, [req]);
            if (i < 0)
            {
                integer type = llGetInventoryType(req);
                if (type == INVENTORY_NONE)
                {
                    llShout(c, "T" + TightListDump(["-4", llList2String(indexes, 0)], "!"));
                }
                else if (type == INVENTORY_NOTECARD)
                {
                    llShout(c, "T!-2");
                    llResetScript();
                }
                else if (type == INVENTORY_ANIMATION)
                {
                    llShout(c, "A" + (string)llGetInventoryKey(req));
                }
                else if (type == INVENTORY_SOUND)
                {
                    llShout(c, "S" + TightListDump([(string)llGetInventoryKey(req), "1.0"], "!"));
                }
                else if (type == INVENTORY_SCRIPT)
                {
                    llShout(c, "T" + TightListDump(["-4", llList2String(indexes, 0)], "!"));
                }
                else
                {
                    llShout(c, "cDownloading " + req);
                    if ((n = llGetOwnerKey(id)) != id)
                        llGiveInventory(n, req);
                    else
                    {
                        llShout(c, "cCannot resolve your key ! Please come to sim secondlife://" + llGetRegionName());
                    }
                }
            }
            else
            {
                llShout(c, "U" + TightListDump([0, 2, llGetInventoryKey(req), (string)llGetKey() + "|" + req], "/"));
            }
        }
        else
        {
            integer n;
            integer max = llList2Integer(sizes, i);
            integer o = llList2Integer(offsets, i);
            for (n = 0; n < max; n = n + 1)
            {
                llShout(c, llList2String(pages, n + o));
            }
        }
    }

    email(string time, string address, string subj, string message, integer left)
    {
        if (subj == "TLTP")
        {
            integer start = llSubStringIndex(message, "\n\n") + 2;
            string a = llGetSubString(message, start, start);
            if (a != "U")
                return;
            list info = TightListParse(llDeleteSubString(message, 0, start));
            name = llList2String(info, 0);
            integer i = llListFindList(indexes, [name]);
            if (i < 0)
            {
                i = llListFindList(tlmlPages, [name]);
                if (i < 0)
                {
                    integer type = llGetInventoryType(name);
                    if (type == INVENTORY_NONE)
                    {
                        sendEmail(llList2String(info, 2), "TLTP", "T" + TightListDump(["-4", llList2String(indexes, 0)], "!"));
                    }
                    else if (type == INVENTORY_NOTECARD)
                    {
                        sendEmail(llList2String(info, 2), "TLTP", "T!-2");
                        llResetScript();
                    }
                    else if (type == INVENTORY_ANIMATION)
                    {
                        sendEmail(llList2String(info, 2), "TLTP", "A" + TightListDump([llGetInventoryKey(name)], "!"));
                    }
                    else if (type == INVENTORY_SOUND)
                    {
                        sendEmail(llList2String(info, 2), "TLTP", "S" + TightListDump([llGetInventoryKey(name), "1.0"], "!"));
                    }
                    else if (type == INVENTORY_SCRIPT)
                    {
                        sendEmail(llList2String(info, 2), "TLTP", "T" + TightListDump(["-4", llList2String(indexes, 0)], "!"));
                    }
                    else
                    {
                        sendEmail(llList2String(info, 2), "TLTP", "cDownloading " + name);
                        string id = llGetOwnerKey(llGetSubString(message, 0, 35));
                        if (id != llGetSubString(message, 0, 35))
                        {
                            llGiveInventory(id, name);
                        }
                        else
                            sendEmail(llList2String(info, 2), "TLTP", "cCannot resolve your key ! Please come to sim secondlife://" + llGetRegionName());
                    }
                }
                else
                {
                    sendEmail(llList2String(info, 2), "TLTP", "U" + TightListDump([0, 2, llGetInventoryKey(name), (string)llGetKey() + "|" + name], "/"));
                }
            }
            else
            {
                string data;
                integer n;
                integer l = llList2Integer(sizes, i);
                integer o = llList2Integer(offsets, i);
                string m = llList2String(pages, o);
                for (n = 1; n < l; ++n)
                {
                    data = llList2String(pages, o + n);
                    if (llStringLength(m) + llStringLength(data) > 800)
                    {
                        sendEmail(llList2String(info, 2), "TLTP", m);
                        m = data;
                    }
                    else
                        m += "\n" + data;
                }
                sendEmail(llList2String(info, 2), "TLTP", m);
            }
        }
        else
            llOwnerSay("Unknown email received: " + subj + ": " + message);
    }
}
