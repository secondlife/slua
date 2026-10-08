list llParticleSystem_list = [];
string byte2hex(integer x)
{
    integer x0 = (x & 15);
    return llGetSubString("0123456789ABCDEF", x0 = ((x >> 4) & 15), x0) + llGetSubString("0123456789ABCDEF", x0, x0);
}

string Unescape(string a)
{
    string b = a;
    integer c = -1;
    integer d;
    integer e;
    integer f = 0;
    string g;
    while (d = llSubStringIndex(b, "\\") + 1)
    {
        g = llGetSubString(b, d, d);
        c += d;
        if ((g == "\"") || (g == "\\"))
            a = llDeleteSubString(a, c, c);
        else if (g == "n")
            a = llInsertString(llDeleteSubString(a, c, c + 1), c, "\n");
        else if (g == "t")
            a = llInsertString(llDeleteSubString(a, c, c + 1), c, "    ");
        else if (g == "r")
        {
            g = "";
            if (d + (e = (integer)("0x" + llGetSubString(b, d + 1, d + 1)) * 2) + 1 >= (f = llStringLength(b)))
                e = (f - d - 2) & -2;
            if (f = e)
            {
                do
                    g = "%" + llGetSubString(b, d + e, d + e + 1) + g;
                while((e -= 2) > 0);
            }
            a = llInsertString(llDeleteSubString(a, c, c + 2 + f), c, g = llUnescapeURL(g));
            c += llStringLength(g);
        }
        else if (g == "u" || (e = (g == "U")))
        {
            a = llDeleteSubString(a, c, c + 5 + e *= 4);
            if (0 < e = (integer)("0x" + llGetSubString(b, d + 1, d + 4 + e)))
            {
                if (e >= 67108864)
                    f = 5;
                else if (e >= 2097152)
                    f = 4;
                else if (e >= 65536)
                    f = 3;
                else if (e >= 2048)
                    f = 2;
                else if (e >= 128)
                    f = 1;
                g = "%" + byte2hex((e >> (6 * f)) | ((16256 >> f) * (0 != f)));
                while (f)
                    g += "%" + byte2hex((((e >> (6 * --f)) | 128) & 191));
                a = llInsertString(a, c++, llUnescapeURL(g));
            }
        }
        b = llDeleteSubString(a, 0, c);
    }
    return a;
}

string flo(float a)
{
    string b = (string)a;
    while (llGetSubString(b, -1, -1) == "0")
        b = llDeleteSubString(b, -1, -1);
    if (llGetSubString(b, -1, -1) == ".")
        return llDeleteSubString(b, -1, -1);
    if (llGetSubString(b, (a < 0), (a < 0) + 1) == "0.")
        return llDeleteSubString(b, (a < 0), (a < 0));
    return b;
}

string vec(vector a)
{
    if (a == ZERO_VECTOR)
        return "";
    return "<" + flo(a.x) + "," + flo(a.y) + "," + flo(a.z) + ">";
}

string rot(quaternion a)
{
    if (a == ZERO_ROTATION)
        return "";
    return "<" + flo(a.x) + "," + flo(a.y) + "," + flo(a.z) + "," + flo(a.s) + ">";
}

string int(integer a)
{
    if (a == 0)
        return "";
    return (string)a;
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

list lis(list a)
{
    integer b = -llGetListLength(a) - 1;
    list c;
    integer d;
    while (++b)
    {
        if ((d = llGetListEntryType(a, b)) == TYPE_FLOAT)
        {
            float e = llList2Float(a, b);
            if (e != 0.00000)
                c += flo(e);
            else
                c += "";
        }
        else if (d == TYPE_VECTOR)
            c += vec(llList2Vector(a, b));
        else if (d == TYPE_ROTATION)
            c += rot(llList2Rot(a, b));
        else if (d == TYPE_INTEGER)
            c += int(llList2Integer(a, b));
        else
            c += llList2String(a, b);
    }
    return c;
}

string hex(integer x)
{
    integer x0 = x & 15;
    string res = llGetSubString("0123456789ABCDEF", x0, x0);
    x = (x >> 4) & 268435455;
    while (x != 0)
    {
        x0 = x & 15;
        res = llGetSubString("0123456789ABCDEF", x0, x0) + res;
        x = x >> 4;
    }
    return res;
}

add(list value, integer mask)
{
    if (llStringLength(llDumpList2String(header + params + value, " ")) > 240)
    {
        store();
        mode = mask | (mode & 257);
    }
    else
        mode = mode | mask;
    params += value;
}

store()
{
    integer a;
    integer b = 0;
    if (llList2String(params, -1) != sep)
        params += mode;
    else
        params = llDeleteSubList(params, -1, -1);
    @loop;
    if (1 + (a = b))
        if (b = 1 + llListFindList(llList2List(params + [sep], a + 1, 200), [sep]))
        {
            b += a;
            params = llListInsertList(llDeleteSubList(params, b - 1, b), [hex(llList2Integer(params, b - 1) | (!a << 5))], a);
            jump loop;
        }
    params = [TightListDump(params, "")];
    commands += params;
    params = [sep];
    if (mode & 256)
        params += cface;
    recycle_mask = recycle_mask | mode;
}

break()
{
    if (mode & -80)
    {
        params += mode;
        recycle_mask = recycle_mask | mode;
        mode = 1;
        params += sep;
    }
}

theFace(integer f)
{
    if (multiplefaces)
        add([f], 256);
    if (llList2Integer(llGetPrimitiveParams([PRIM_FULLBRIGHT, f]), 0))
        mode = mode | 16;
    if (llGetTexture(f) != "5748decc-f629-461c-9a36-a35a221fe21f")
        add([llGetTexture(f)], 512);
    vector t_v = llGetTextureScale(f);
    if (t_v != <1.00000, 1.00000, 0.00000>)
        add([vec(t_v)], 1024);
    t_v = llGetTextureOffset(f);
    if (t_v != ZERO_VECTOR)
        add([vec(t_v)], 2048);
    else
        mode = mode | 2;
    float t_f = llGetTextureRot(f);
    if (t_f != 0.00000)
        add([flo(t_f)], 4096);
    else
        mode = mode | 2;
    t_v = llGetColor(f);
    if (t_v != <1.00000, 1.00000, 1.00000>)
        add([vec(t_v)], 8192);
    else
        mode = mode | 4;
    t_f = llGetAlpha(f);
    if (f != 1.00000)
        add([flo(t_f)], 16384);
    else
        mode = mode | 4;
    list t_l = llGetPrimitiveParams([PRIM_BUMP_SHINY, f]);
    integer t_i = llList2Integer(t_l, 0);
    if (t_i != PRIM_SHINY_NONE)
        add([t_i], 32768);
    else
        mode = mode | 8;
    t_i = llList2Integer(t_l, 1);
    if (t_i != PRIM_BUMP_NONE)
        add([t_i], 65536);
    else
        mode = mode | 8;
}

checkFaces()
{
    integer max = llGetNumberOfSides();
    multiplefaces = FALSE;
    string texture = llGetTexture(0);
    vector color = llGetColor(0);
    float alpha = llGetAlpha(0);
    list fullbrights = llGetPrimitiveParams([PRIM_FULLBRIGHT, ALL_SIDES]);
    integer fullbright = llList2Integer(fullbrights, 0);
    list bump_shiny = llGetPrimitiveParams([PRIM_BUMP_SHINY, ALL_SIDES]);
    integer bump = llList2Integer(bump_shiny, 1);
    integer shiny = llList2Integer(bump_shiny, 0);
    integer i = 1;
    for (; i < max && !multiplefaces; ++i)
    {
        if (llGetTexture(i) != texture)
            multiplefaces = TRUE;
        if (llGetColor(i) != color)
            multiplefaces = TRUE;
        if (llGetAlpha(i) != alpha)
            multiplefaces = TRUE;
        if (llList2Integer(fullbrights, i) != fullbright)
            multiplefaces = TRUE;
        if (llList2Integer(bump_shiny, i * 2 + 1) != bump)
            multiplefaces = TRUE;
        if (llList2Integer(bump_shiny, i * 2) != shiny)
            multiplefaces = TRUE;
    }
    if (!multiplefaces)
    {
        mode = mode | 1;
        cface = ALL_SIDES;
    }
    else
    {
        cface = 0;
    }
    theFace(0);
}

checkPrim()
{
    list type = llGetPrimitiveParams([PRIM_TYPE]);
    if (llList2Integer(type, 0) == 0)
    {
        if (llList2Integer(type, 1) == 0)
        {
            if (llList2Vector(type, 2) == <0.00000, 1.00000, 0.00000>)
            {
                if (llList2Float(type, 3) == 0.00000)
                {
                    if (llList2Vector(type, 4) == ZERO_VECTOR)
                    {
                        if (llList2Vector(type, 5) == <1.00000, 1.00000, 0.00000>)
                        {
                            if (llList2Vector(type, 6) == ZERO_VECTOR)
                            {
                                return;
                            }
                        }
                    }
                }
            }
        }
    }
    add(lis(type), 67108864);
}

string sep;
integer mode;
integer cface;
integer multiplefaces;
list params;
list header;
integer recycle_mask;
list commands;
default
{
    state_entry()
    {
        llSetText(Unescape(""), <1.00000, 1.00000, 1.00000>, 1.00000);
        llTargetOmega(<0.00000, 0.00000, 0.00000>, 0.00000, 0.00000);
        llParticleSystem(llParticleSystem_list);
        llSetTextureAnim(0, -1, 2, 2, 0.00000, 3.00000, 0.100000);
        llOwnerSay("--------------------------------------");
        sep = llUnescapeURL("%01");
        if ((llGetObjectPermMask(MASK_OWNER) & 57344) != 57344)
        {
            llOwnerSay("You cannot clone an object you do not have full permission on");
            return;
        }
        header = [llGetLinkNumber(), "url"];
        params = [sep];
        checkFaces();
        add([vec(llGetScale())], 131072);
        if (llGetLinkNumber() >= 2)
            add([vec(llGetLocalPos())], 262144);
        quaternion local = llGetLocalRot();
        if (local != ZERO_ROTATION)
            add([rot(llGetLocalRot())], 1048576);
        if (0)
            add([""], 8388608);
        if (0)
            add([vec(<1.00000, 1.00000, 1.00000>), flo(1.00000)], 16777216);
        if (0)
            add([vec(<0.00000, 0.00000, 0.00000>), flo(0.00000), flo(0.00000)], 33554432);
        checkPrim();
        if (0)
            add([TightListDump(lis(llParticleSystem_list), "*")], 268435456);
        integer t;
        if (multiplefaces)
        {
            t = llGetNumberOfSides();
            cface = 1;
            while (cface < t)
            {
                break();
                theFace(cface);
                ++cface;
            }
        }
        if (0)
        {
            break();
            add([-1, 0, 2, 2, 0.00000, 3.00000, 0.100000], 2097408);
        }
        store();
        t = llGetListLength(commands);
        integer c;
        string d;
        string e;
        string f;
        if (t > 1)
            d = hex(recycle_mask);
        while (c < t)
        {
            e = llDumpList2String(llParseString2List(llList2String(commands, c), [sep], []), d);
            f = TightListDump(header, llGetSubString(e, 0, 0));
            llOwnerSay("T" + f + e);
            if (++c == 1)
                header = [llGetLinkNumber(), ""];
        }
    }
}
