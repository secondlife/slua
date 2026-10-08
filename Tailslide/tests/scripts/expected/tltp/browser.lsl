list current_url;
list history;
key source;
integer is_on;
integer browsing_channel;
integer browsing_handle;
integer channel;
integer handle;
integer action;
list advertised_names;
list advertised_urls;
list bookmarked_names;
list bookmarked_urls;
integer bkm_part;
integer adv_part;
key query;
integer line;
string anim;
key page_key;
key page_query;
integer page_line;
key joint_page_key;
key joint_page_query;
integer joint_page_line;
integer max_prim_used;
integer emailer;
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

go(string url)
{
    if (0)
        llOwnerSay(url);
    list t = TightListParse(url) + current_url;
    integer argc = llGetListLength(t);
    if (argc < 5)
    {
        llOwnerSay("Missing arguments in URL: " + url);
        return;
    }
    if (url == llList2String(history, 0))
    {
        if (llGetListLength(history) > 1)
        {
            history = llList2List(history, 1, -1);
        }
        else
            history = [];
    }
    else
        history = TightListDump(current_url, "!") + history;
    if (llGetListLength(history) > 12)
        history = llList2List(history, 0, 11);
    current_url = llDeleteSubList(t, -4, argc - 9);
    request(current_url);
}

request(list u)
{
    if (llGetListLength(u) != 4)
    {
        llOwnerSay("Bad URL format");
        return;
    }
    llSetText("Browsing " + llList2String(u, 3), <0.00000, 1.00000, 1.00000>, 0.800000);
    if (llList2String(u, 1) == "0")
    {
        if (llList2String(u, 2) == "0")
        {
            llOwnerSay("Bad server channel.");
            return;
        }
        source = "";
        llListenRemove(browsing_handle);
        do
            ;
        while(!(browsing_channel = (integer)llFrand(65536.0) | ((integer)llFrand(65536.0) << 16)));
        browsing_handle = llListen(browsing_channel, "", "", "");
        llShout((integer)llList2String(u, 2), "U" + TightListDump([llList2String(u, 0), "0", (string)browsing_channel, llGetObjectName()], "&"));
    }
    else if (llList2String(u, 1) == "1")
    {
        source = "";
        if (emailer <= max_prim_used + 2)
            emailer = llGetNumberOfPrims();
        llMessageLinked(emailer, 1100, TightListDump([emailer, llList2String(u, 2), "TLTP"], ";"), (key)("U" + TightListDump([llList2String(u, 0), "1", llGetKey(), llGetObjectName()], "&")));
        emailer = emailer - 1;
    }
    else if (llList2String(u, 1) == "2")
    {
        joint_page_key = llList2String(u, 2);
        joint_page_line = (integer)llList2String(u, 0);
        joint_page_query = llGetNotecardLine(joint_page_key, joint_page_line);
    }
    else
    {
        llOwnerSay("Unknown transport method for URL: " + llList2CSV(u));
        return;
    }
}

parse(string command, string hook)
{
    string a = llGetSubString(command, 0, 0);
    command = llDeleteSubString(command, 0, 0);
    if (a == "U")
    {
        llOwnerSay("Redirected.");
        go(command);
    }
    else if (a == "T")
    {
        tlml(command);
    }
    else if (a == "t")
    {
        page_key = command;
        page_line = 0;
        page_query = llGetNotecardLine(page_key, page_line);
    }
    else if (a == "J")
    {
        llMessageLinked(LINK_SET, 6000, command, hook);
    }
    else if (a == "x" || a == "X")
    {
        integer c = (a == "x") * 3000 + (a == "X") * 3100;
        llMessageLinked(LINK_SET, c, command, (key)TightListDump([llList2String(current_url, 2), llList2String(current_url, 0)], "*"));
    }
    else if (a == "A")
    {
        anim = command;
        llRequestPermissions(llGetOwner(), PERMISSION_TRIGGER_ANIMATION);
    }
    else if (a == "S")
    {
        list l = TightListParse(command);
        if (llGetListLength(l) < 2)
        {
            llPlaySound(llList2String(l, 0), 0.500000);
        }
        else
            llPlaySound(llList2String(l, 0), (float)llList2String(l, 1));
    }
    else if (a == "R")
    {
        list rpc = TightListParse(command);
        llMessageLinked(LINK_THIS, 4400, llList2String(rpc, 0), (key)TightListDump(llDeleteSubList(rpc, 0, 0), "|"));
    }
    else if (a == "C")
    {
        list chat = TightListParse(command);
        if (llGetListLength(chat) > 1)
        {
            llShout((integer)llList2String(chat, 0), llList2String(chat, 1));
        }
        else
            llSay(0, llList2String(chat, 0));
    }
    else if (a == "c")
    {
        llOwnerSay(command);
    }
    else if (a == "W")
    {
        if ((float)command > 10.0000)
        {
            llSleep(10.0000);
        }
        else
            llSleep((float)command);
    }
    else if (a == "N")
    {
        list com = TightListParse(command);
        llMessageLinked(LINK_SET, 5200, llList2String(com, 0), TightListDump(llDeleteSubList(com, 0, 0), "*"));
    }
}

tlml(string t)
{
    integer ind = llSubStringIndex(llDeleteSubString(t, 0, 0), llGetSubString(t, 0, 0));
    integer target = (integer)llGetSubString(t, 1, ind);
    if (target >= 0)
    {
        if (target > llGetNumberOfPrims() - 2)
        {
            llOwnerSay("Not enough prims to display this page.");
            return;
        }
        if (target > max_prim_used)
            max_prim_used = target;
        llMessageLinked(target + 2, 1000, llDeleteSubString(t, 0, ind), (key)TightListDump([TightListDump(current_url, "#"), llGetLocalRot(), llList2String(current_url, 2)], "&"));
    }
    else
    {
        if (target == -99)
        {
            clear();
        }
        else if (target == -1)
        {
            llOwnerSay("Server is unavailable !");
        }
        else if (target == -2)
        {
            llOwnerSay("Server error !");
        }
        else if (target == -3)
        {
            llOwnerSay("Access denied !");
        }
        else if (target == -4)
        {
            llOwnerSay("Page not found !");
        }
        else if (target == -97)
        {
            float delay = (float)llDeleteSubString(t, 0, ind);
            if (delay > 10.0000)
                delay = 10.0000;
            llSleep(delay);
        }
        else if (target != -98)
        {
            llOwnerSay("Server response not recognized: " + t);
        }
    }
}

clear()
{
    llMessageLinked(LINK_ALL_OTHERS, 8999, "nevermind", "00000000-0000-0000-0000-000000000000");
    max_prim_used = 0;
}

on()
{
    is_on = TRUE;
    llSetColor(<1.00000, 1.00000, 0.00000>, ALL_SIDES);
    llListenRemove(0);
    0 == llListen(-9, "", "", "");
    integer n;
    for (n = 0; n < max_prim_used; ++n)
        llSetLinkAlpha(n + 2, 1.00000, ALL_SIDES);
    llSetTimerEvent(5.00000);
}

off()
{
    is_on = FALSE;
    llSetColor(<0.200000, 0.200000, 0.200000>, ALL_SIDES);
    llSetText("", <0.00000, 0.00000, 0.00000>, 0.00000);
    llListenRemove(0);
    llListenRemove(browsing_handle);
    advertised_urls = [];
    advertised_names = [];
    source = "";
    browsing_handle = 0;
    page_query = "00000000-0000-0000-0000-000000000000";
    query = "00000000-0000-0000-0000-000000000000";
    joint_page_query = "00000000-0000-0000-0000-000000000000";
    integer n;
    for (n = 0; n < max_prim_used; ++n)
        llSetLinkAlpha(n + 2, 0.00000, ALL_SIDES);
    llSetTimerEvent(0.00000);
}

config(string msg)
{
    if (action == 1)
    {
        if (msg == "Back")
        {
            action = 0;
            menu();
        }
        else if (msg == "More")
        {
            bkm_part = (bkm_part + 1) % llGetListLength(bookmarked_names);
            menu();
        }
        else
        {
            integer i = llListFindList(bookmarked_names, [msg]);
            if (i < 0)
            {
                llOwnerSay("Bookmark not found.");
            }
            else
            {
                go(llList2String(bookmarked_urls, i));
            }
            llListenRemove(handle);
        }
    }
    else if (action == 2)
    {
        if (msg == "Back")
        {
            action = 0;
            menu();
        }
        else if (msg == "More")
        {
            adv_part = (adv_part + 1) % llGetListLength(advertised_names);
            menu();
        }
        else
        {
            integer i = llListFindList(advertised_names, [msg]);
            if (i < 0)
            {
                llOwnerSay("Server not found.");
            }
            else
            {
                go(llList2String(advertised_urls, i));
            }
            llListenRemove(handle);
        }
    }
    else
    {
        if (msg == "Bookmarks")
        {
            action = 1;
            menu();
            return;
        }
        if (msg == "Connect")
        {
            action = 2;
            menu();
            return;
        }
        if (msg == "Reload")
        {
            request(current_url);
        }
        if (msg == "< Back")
        {
            if (llGetListLength(history) > 1)
            {
                go(llList2String(history, 0));
            }
            else
                llOwnerSay("Can't go further back.");
        }
        else if (msg == "On")
        {
            on();
        }
        else if (msg == "Off")
        {
            off();
        }
        else if (msg == "Close")
        {
            llListenRemove(browsing_handle);
            browsing_handle = 0;
            source = "";
            llSetText("", <0.00000, 0.00000, 0.00000>, 0.00000);
            clear();
        }
        llListenRemove(handle);
    }
}

menu()
{
    string opt;
    list btns;
    if (action == 0)
    {
        opt = "Detected servers: " + (string)llGetListLength(advertised_urls);
        if (is_on)
        {
            btns = ["Off"];
        }
        else
        {
            btns = ["On"];
        }
        btns += ["ExitMenu"];
        if (llGetListLength(history) > 1)
            btns += ["< Back"];
        if (llGetListLength(bookmarked_names) > 0)
            btns += ["Bookmarks"];
        if (llGetListLength(advertised_urls) > 0)
            btns += ["Connect"];
        if (llStringLength(source))
            btns += ["Close", "Reload"];
    }
    else if (action == 1)
    {
        opt = "Connect to:";
        if (llGetListLength(bookmarked_names) < 12)
        {
            btns = bookmarked_names + ["Back"];
        }
        else
        {
            btns = llList2List(bookmarked_names, bkm_part * 10, bkm_part * 10 + 9) + ["More", "Back"];
        }
    }
    else
    {
        opt = "Connect to:";
        if (llGetListLength(advertised_names) < 12)
        {
            btns = advertised_names + ["Back"];
        }
        else
        {
            btns = llList2List(advertised_names, adv_part * 10, adv_part * 10 + 9) + ["More", "Back"];
        }
    }
    llDialog(llGetOwner(), opt, btns, channel);
}

default
{
    state_entry()
    {
        on();
        browsing_handle = 0;
        bkm_part = 0;
        adv_part = 0;
        current_url = ["", "", "", ""];
        llSetText("", <0.00000, 0.00000, 0.00000>, 0.00000);
    }

    changed(integer c)
    {
        if (c & CHANGED_INVENTORY)
        {
            if (llGetInventoryNumber(INVENTORY_NOTECARD) > 0)
            {
                line = 0;
                llOwnerSay("Adding bookmarks...");
                query = llGetNotecardLine(llGetInventoryName(INVENTORY_NOTECARD, 0), line);
            }
        }
    }

    dataserver(key id, string data)
    {
        if (id == query)
        {
            if (data != EOF)
            {
                if (llGetSubString(data, 0, 0) != "U")
                    return;
                list l = TightListParse(llDeleteSubString(data, 0, 0));
                if (llGetListLength(l) != 4)
                {
                    llOwnerSay("Bad bookmark skipped: " + data);
                }
                else
                {
                    if (llListFindList(bookmarked_urls, [data]) < 0)
                    {
                        string name_to_bkm = llList2String(l, 3);
                        if (llStringLength(name_to_bkm) > 24)
                            name_to_bkm = llGetSubString(name_to_bkm, 0, 23);
                        bookmarked_names += [name_to_bkm];
                        bookmarked_urls += [llDeleteSubString(data, 0, 0)];
                    }
                    else
                        llOwnerSay("Redundant bookmark skipped: " + data);
                }
                line = line + 1;
                query = llGetNotecardLine(llGetInventoryName(INVENTORY_NOTECARD, 0), line);
            }
            else
            {
                llRemoveInventory(llGetInventoryName(INVENTORY_NOTECARD, 0));
                if (llGetInventoryNumber(INVENTORY_NOTECARD) > 0)
                {
                    line = 0;
                    llOwnerSay("Adding more bookmarks...");
                    query = llGetNotecardLine(llGetInventoryName(INVENTORY_NOTECARD, 0), line);
                }
                else
                    llOwnerSay("Done adding bookmarks.");
            }
        }
        else if (id == page_query)
        {
            if (data != EOF)
            {
                parse(data, "");
                page_line = page_line + 1;
                page_query = llGetNotecardLine(page_key, page_line);
            }
        }
        else if (id == joint_page_query)
        {
            llMessageLinked(LINK_ALL_OTHERS, 6000, data, (key)TightListDump([TightListDump([joint_page_line, 2, joint_page_key, llList2String(current_url, 3)], "#"), llGetLocalRot(), joint_page_key], "&"));
        }
    }

    timer()
    {
        if (llGetTime() > 10.0000)
        {
            advertised_names = [];
            advertised_urls = [];
            llResetTime();
            llSetColor(<1.00000, 1.00000, 0.00000>, ALL_SIDES);
        }
        llGetNextEmail("", "");
    }

    run_time_permissions(integer p)
    {
        if (p & PERMISSION_TRIGGER_ANIMATION)
            llStartAnimation(anim);
    }

    touch_start(integer c)
    {
        if (llDetectedKey(0) != llGetOwner())
            return;
        if ((llGetPermissions() & PERMISSION_TRIGGER_ANIMATION) && (anim != ""))
        {
            llStopAnimation(anim);
            anim = "";
        }
        llStopSound();
        action = 0;
        adv_part = 0;
        bkm_part = 0;
        llListenRemove(handle);
        channel = (integer)llFrand(500000000) - 800000000;
        handle = llListen(channel, "", llGetOwner(), "");
        llResetTime();
        menu();
    }

    email(string time, string address, string subj, string message, integer left)
    {
        integer n = 0;
        if (0)
            llOwnerSay("Received email: " + subj + ": " + message);
        if (subj == "TLTP")
        {
            n = llSubStringIndex(message, "\n\n");
            message = llDeleteSubString(message, 0, n + 1);
            source = (key)llGetSubString(address, 0, llStringLength(message) - 19);
            if (llGetSubString(address, -20, -1) == "@lsl.secondlife.com")
            {
                current_url = llListReplaceList(current_url, [(string)source], 2, 2);
            }
            else
                current_url = llListReplaceList(current_url, [address], 2, 2);
            list commands = llParseStringKeepNulls(message, ["\n"], []);
            integer m = llGetListLength(commands);
            for (n = 0; n < m; ++n)
                parse(llList2String(commands, n), "");
            if (left > 0)
                llGetNextEmail("", "");
        }
    }

    listen(integer chan, string name, key id, string msg)
    {
        if (chan == channel)
        {
            llResetTime();
            config(msg);
        }
        else if (chan == -9)
        {
            llResetTime();
            if (llGetSubString(msg, 0, 0) == "U")
            {
                if (llListFindList(advertised_urls, [llDeleteSubString(msg, 0, 0)]) == -1)
                {
                    list temp = TightListParse(llDeleteSubString(msg, 0, 0));
                    if (llGetListLength(temp) >= 4)
                    {
                        llSetColor(<1.00000, 0.00000, 0.00000>, ALL_SIDES);
                        string na = llList2String(temp, 3);
                        if (llStringLength(na) > 24)
                            na = llGetSubString(na, 0, 23);
                        llOwnerSay("Found server: " + na);
                        advertised_names += [na];
                        advertised_urls += [llDeleteSubString(msg, 0, 0)];
                    }
                    else if (0)
                        llOwnerSay("Incomplete broadcasted URL");
                }
            }
        }
        else
        {
            llResetTime();
            if (source)
            {
                if (source == id)
                    parse(msg, "");
            }
            else
            {
                source = id;
                parse(msg, "");
            }
        }
    }

    link_message(integer part, integer code, string msg, key id)
    {
        if (code == 2000)
        {
            parse(msg, id);
        }
    }
}
