list HEX_CHARACTERS = ["0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "a", "b", "c", "d", "e", "f"];
key rpc_channel;
key rpc_msg_key;
integer is_channel_available;
list local_names;
list local_locations;
list remote_names;
list remote_locations;
integer global_message_id;
list message_queue;
list messages_sent;
integer next_name_pos = -1;
integer next_location_pos = -1;
string int2hex(integer i)
{
    return llList2String(HEX_CHARACTERS, i);
}

string int2hex2(integer i)
{
    return llList2String(HEX_CHARACTERS, i / 16) + llList2String(HEX_CHARACTERS, i % 16);
}

integer hex2int(string s)
{
    return (integer)("0x" + s);
}

flash(string prim, integer on)
{
    integer i;
    integer prims = llGetNumberOfPrims() + 1;
    list l;
    if (on)
        on = 2;
    else
        on = 1;
    for (i = 2; i < prims; ++i)
    {
        l = llParseString2List(llGetLinkName(i), [":"], []);
        if (llList2String(l, 0) == prim)
        {
            llSetLinkColor(i, (vector)llList2String(l, on), ALL_SIDES);
            return;
        }
    }
}

reset()
{
    integer i;
    global_message_id = 0;
    remote_names = [];
    remote_locations = [];
    for (i = 0; i < 14; i++)
    {
        remote_names += "?";
        remote_locations += "?";
        if (llGetListLength(local_names) > i)
            queue_message(0, i, 15, llList2String(local_names, i));
        if (llGetListLength(local_locations) > i)
            queue_message(0, 15, i, llList2String(local_locations, i));
    }
}

queue_message(integer pos, integer name, integer loc, string data)
{
    string message = int2hex(name) + int2hex(loc) + int2hex2(llStringLength(data));
    data = llDumpList2String(llParseStringKeepNulls(data, ["&"], []), "&amp;");
    data = llDumpList2String(llParseStringKeepNulls(data, ["<"], []), "&lt;");
    data = llDumpList2String(llParseStringKeepNulls(data, [">"], []), "&gt;");
    message += data;
    if (pos == 0)
        message_queue = message + message_queue;
    else
        message_queue = llListInsertList(message_queue, [message], pos);
}

handle_request(string data)
{
    integer pos = 0;
    integer len = llStringLength(data);
    integer name;
    integer location;
    integer msgdatalen;
    string msgdata;
    flash("recv", 1);
    while (pos < len)
    {
        name = hex2int(llGetSubString(data, pos, pos));
        location = hex2int(llGetSubString(data, ++pos, pos));
        msgdatalen = hex2int(llGetSubString(data, ++pos, ++pos));
        msgdata = llGetSubString(data, ++pos, pos += (msgdatalen - 1));
        ++pos;
        if (name == 15)
            remote_locations = llListReplaceList(remote_locations, [msgdata], location, location);
        else if (location == 15)
            remote_names = llListReplaceList(remote_names, [msgdata], name, name);
        else
            send_message(name, location, msgdata);
    }
    if (is_channel_available)
    {
        llResetTime();
    }
    flash("recv", 0);
}

send_message(integer name, integer location, string message)
{
    string header = "<[" + llList2String(remote_locations, location) + "] " + llList2String(remote_names, name) + "> ";
    string b64_msg = "TextBase64::" + header + llStringToBase64(" " + message);
    if (llStringLength(b64_msg) < 255)
        llSay(956458772, b64_msg);
    else
        llSay(956458772, "Text::" + header + message);
}

process_queue(integer force)
{
    string data = "";
    integer datalen = 0;
    string msg;
    integer msglen;
    integer to_pop = 0;
    messages_sent = [];
    while (llGetListLength(message_queue) > to_pop)
    {
        msg = llList2String(message_queue, to_pop);
        msglen = llStringLength(msg);
        if ((datalen + msglen) > 255)
            jump process_queue__break1;
        to_pop += 1;
        messages_sent += [msg];
        datalen += msglen;
        data += msg;
    }
    @process_queue__break1;
    if (to_pop > 0)
    {
        message_queue = llDeleteSubList(message_queue, 0, to_pop - 1);
    }
    if (datalen > 0 || force)
    {
        llSetTimerEvent(0);
        flash("open", 0);
        flash("send", 1);
        llRemoteDataReply(rpc_channel, rpc_msg_key, data, global_message_id);
        flash("send", 0);
        llSetTimerEvent(1);
        is_channel_available = FALSE;
    }
}

add_to_queue(string name, string location, string message)
{
    integer iname = get_local_name(name);
    integer iloc = get_local_location(location);
    queue_message(2147483647, iname, iloc, message);
}

integer get_local_name(string name)
{
    integer pos = llListFindList(local_names, [name]);
    if (pos != -1)
        return pos;
    pos = llGetListLength(local_names);
    if (pos < 14)
    {
        local_names += name;
        queue_message(2147483647, pos, 15, name);
        return pos;
    }
    next_name_pos = (next_name_pos + 1) % 14;
    local_names = llListReplaceList(local_names, [name], next_name_pos, next_name_pos);
    queue_message(2147483647, next_name_pos, 15, name);
    return next_name_pos;
}

integer get_local_location(string name)
{
    integer pos = llListFindList(local_locations, [name]);
    if (pos != -1)
        return pos;
    pos = llGetListLength(local_locations);
    if (pos < 14)
    {
        local_locations += name;
        queue_message(2147483647, 15, pos, name);
        return pos;
    }
    next_location_pos = (next_location_pos + 1) % 14;
    local_locations = llListReplaceList(local_locations, [name], next_location_pos, next_location_pos);
    queue_message(2147483647, 15, next_location_pos, name);
    return next_location_pos;
}

default
{
    state_entry()
    {
        reset();
        llOpenRemoteDataChannel();
        llListen(956458772, "", NULL_KEY, "");
        llSetTimerEvent(1);
        llResetTime();
    }

    remote_data(integer type, key channel, key msg_key, string sender, integer message_id, string data)
    {
        if (type == REMOTE_DATA_CHANNEL)
        {
            rpc_channel = channel;
            llOwnerSay(llGetScriptName() + ": " + (string)rpc_channel);
            is_channel_available = FALSE;
            return;
        }
        else if (type == REMOTE_DATA_REQUEST)
        {
            if (global_message_id == 0 && message_id != 0 && message_id != 1)
            {
                process_queue(TRUE);
            }
            else
            {
                rpc_msg_key = msg_key;
                is_channel_available = TRUE;
                flash("open", 1);
                if (message_id != (global_message_id + 1))
                {
                    message_queue = messages_sent + message_queue;
                }
                if (message_id == 0)
                {
                    reset();
                }
                global_message_id = message_id + 1;
                handle_request(data);
            }
        }
    }

    listen(integer channel, string name, key id, string msg)
    {
        if (id == llGetKey())
            return;
        list l = llParseStringKeepNulls(msg, [], ["<", "[", "]", ">"]);
        string type = llList2String(l, 0);
        string mloc = llList2String(l, 4);
        string mname = llDeleteSubString(llList2String(l, 6), 0, 0);
        string msg = llDeleteSubString(llDumpList2String(llList2List(l, 8, -1), ""), 0, 0);
        if (type == "TextBase64::")
            msg = llDeleteSubString(llBase64ToString(msg), 0, 0);
        add_to_queue(mname, mloc, msg);
    }

    timer()
    {
        if (is_channel_available)
        {
            if (llGetTime() > 5)
            {
                process_queue(TRUE);
            }
            else
            {
                process_queue(FALSE);
            }
        }
    }
}
