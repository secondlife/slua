key gFontTexture = "b2e7394f-5e54-aa12-6e1c-ef327b6bed9e";
string gCharIndex;
integer gCellChannel = -1;
integer gCellCharPosition = 0;
integer gCellUseFading = FALSE;
float gCellHoldDelay = 1.00000;
ResetCharIndex()
{
    gCharIndex = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`";
    gCharIndex += "abcdefghijklmnopqrstuvwxyz{|}~";
    gCharIndex += "\n\n\n\n\n";
}

vector GetGridOffset(integer index)
{
    integer Row = index / 10;
    integer Col = index % 10;
    return <-0.450000 + 0.100000 * Col, 0.450000 - 0.100000 * Row, 0.00000>;
}

ShowChars(vector grid_offset1, vector grid_offset2, vector grid_offset3, vector grid_offset4, vector grid_offset5)
{
    llSetPrimitiveParams([PRIM_TEXTURE, 3, (string)gFontTexture, <0.120000, 0.100000, 0.00000>, grid_offset1 + <0.0370000, 0.00000, 0.00000>, 0.00000, PRIM_TEXTURE, 7, (string)gFontTexture, <0.0500000, 0.100000, 0.00000>, grid_offset2, 0.00000, PRIM_TEXTURE, 4, (string)gFontTexture, <-0.740000, 0.100000, 0.00000>, grid_offset3 - <0.244000, 0.00000, 0.00000>, 0.00000, PRIM_TEXTURE, 6, (string)gFontTexture, <0.0500000, 0.100000, 0.00000>, grid_offset4, 0.00000, PRIM_TEXTURE, 1, (string)gFontTexture, <0.120000, 0.100000, 0.00000>, grid_offset5 - <0.0370000, 0.00000, 0.00000>, 0.00000]);
}

RenderString(string str)
{
    vector GridOffset1 = GetGridOffset(llSubStringIndex(gCharIndex, llGetSubString(str, 0, 0)));
    vector GridOffset2 = GetGridOffset(llSubStringIndex(gCharIndex, llGetSubString(str, 1, 1)));
    vector GridOffset3 = GetGridOffset(llSubStringIndex(gCharIndex, llGetSubString(str, 2, 2)));
    vector GridOffset4 = GetGridOffset(llSubStringIndex(gCharIndex, llGetSubString(str, 3, 3)));
    vector GridOffset5 = GetGridOffset(llSubStringIndex(gCharIndex, llGetSubString(str, 4, 4)));
    ShowChars(GridOffset1, GridOffset2, GridOffset3, GridOffset4, GridOffset5);
}

RenderWithEffects(string str)
{
    vector GridOffset1 = GetGridOffset(llSubStringIndex(gCharIndex, llGetSubString(str, 0, 0)));
    vector GridOffset2 = GetGridOffset(llSubStringIndex(gCharIndex, llGetSubString(str, 1, 1)));
    vector GridOffset3 = GetGridOffset(llSubStringIndex(gCharIndex, llGetSubString(str, 2, 2)));
    vector GridOffset4 = GetGridOffset(llSubStringIndex(gCharIndex, llGetSubString(str, 3, 3)));
    vector GridOffset5 = GetGridOffset(llSubStringIndex(gCharIndex, llGetSubString(str, 4, 4)));
    llSetAlpha(0.0500000, ALL_SIDES);
    ShowChars(GridOffset1, GridOffset2, GridOffset3, GridOffset4, GridOffset5);
    float Alpha;
    for (Alpha = 0.100000; Alpha <= 1.00000; Alpha += 0.0500000)
        llSetAlpha(Alpha, ALL_SIDES);
    if (gCellHoldDelay < 0.00000)
        return;
    llSleep(gCellHoldDelay);
    for (Alpha = 0.950000; Alpha >= 0.0500000; Alpha -= 0.0500000)
        llSetAlpha(Alpha, ALL_SIDES);
    llSetTexture("701917a8-d614-471f-13dd-5f4644e36e3c", ALL_SIDES);
}

RenderExtended(string str)
{
    list Parsed = llParseString2List(str, [], ["\\e"]);
    integer ParsedLen = llGetListLength(Parsed);
    list Indices;
    integer IndicesLeft = 5;
    integer i;
    string Token;
    integer Clipped;
    integer LastWasEscapeSequence = FALSE;
    for (i = 0; i < ParsedLen && IndicesLeft > 0; i++)
    {
        Token = llList2String(Parsed, i);
        if (Token == "\\e")
        {
            LastWasEscapeSequence = TRUE;
        }
        else
        {
            Clipped = FALSE;
            integer TokenLength = llStringLength(Token);
            if (TokenLength > IndicesLeft)
            {
                Token = llGetSubString(Token, 0, IndicesLeft - 1);
                TokenLength = llStringLength(Token);
                IndicesLeft = 0;
                Clipped = TRUE;
            }
            else
                IndicesLeft -= TokenLength;
            if (LastWasEscapeSequence)
            {
                Indices += [llSubStringIndex("12345", llGetSubString(Token, 0, 0)) + 95];
                integer j;
                for (j = 1; j < TokenLength; j++)
                    Indices += [llSubStringIndex(gCharIndex, llGetSubString(Token, j, j))];
            }
            else
            {
                integer j;
                for (j = 0; j < TokenLength; j++)
                    Indices += [llSubStringIndex(gCharIndex, llGetSubString(Token, j, j))];
            }
            LastWasEscapeSequence = FALSE;
        }
    }
    vector GridOffset1 = GetGridOffset(llList2Integer(Indices, 0));
    vector GridOffset2 = GetGridOffset(llList2Integer(Indices, 1));
    vector GridOffset3 = GetGridOffset(llList2Integer(Indices, 2));
    vector GridOffset4 = GetGridOffset(llList2Integer(Indices, 3));
    vector GridOffset5 = GetGridOffset(llList2Integer(Indices, 4));
    ShowChars(GridOffset1, GridOffset2, GridOffset3, GridOffset4, GridOffset5);
}

integer ConvertIndex(integer index)
{
    if (index >= 32)
        index -= 32;
    else
    {
        if (index > 15)
            index = 15;
        index += 94;
    }
    return index;
}

default
{
    state_entry()
    {
        ResetCharIndex();
    }

    link_message(integer sender, integer channel, string data, key id)
    {
        if (channel == 204000)
        {
            RenderString(data);
            return;
        }
        if (channel == 204001)
        {
            RenderExtended(data);
            return;
        }
        if (channel == gCellChannel)
        {
            string TextToRender = llGetSubString(data, gCellCharPosition, gCellCharPosition + 4);
            if (gCellUseFading)
                RenderWithEffects(TextToRender);
            else
                RenderString(TextToRender);
            return;
        }
        if (channel == 204002)
        {
            list Parsed = llCSV2List(data);
            integer i;
            for (i = 0; i < llGetListLength(Parsed); i += 2)
            {
                integer Index1 = ConvertIndex(llList2Integer(Parsed, i));
                integer Index2 = ConvertIndex(llList2Integer(Parsed, i + 1));
                string Value1 = llGetSubString(gCharIndex, Index1, Index1);
                string Value2 = llGetSubString(gCharIndex, Index2, Index2);
                gCharIndex = llDeleteSubString(gCharIndex, Index1, Index1);
                gCharIndex = llInsertString(gCharIndex, Index1, Value2);
                gCharIndex = llDeleteSubString(gCharIndex, Index2, Index2);
                gCharIndex = llInsertString(gCharIndex, Index2, Value1);
            }
            return;
        }
        if (channel == 204003)
        {
            ResetCharIndex();
            return;
        }
        if (channel == 204004)
        {
            list Parsed = llCSV2List(data);
            gCellChannel = (integer)llList2String(Parsed, 0);
            gCellCharPosition = (integer)llList2String(Parsed, 1);
            gCellUseFading = (integer)llList2String(Parsed, 2);
            gCellHoldDelay = (float)llList2String(Parsed, 3);
            return;
        }
        if (channel == 204005)
        {
            gFontTexture = id;
            llSetTexture(gFontTexture, 3);
            llSetTexture(gFontTexture, 7);
            llSetTexture(gFontTexture, 4);
            llSetTexture(gFontTexture, 6);
            llSetTexture(gFontTexture, 1);
            return;
        }
        if (channel == 204006)
        {
            vector Scale = llGetScale();
            float Thickness = (float)data;
            integer ThisLink = llGetLinkNumber();
            if (ThisLink != 0 || ThisLink != 1)
            {
                vector Up = llRot2Up(llGetLocalRot());
                float DistanceToMove = Thickness / 2.00000 - Scale.z / 2.00000;
                vector Pos = llGetLocalPos();
                llSetPos(Pos + DistanceToMove * Up);
            }
            Scale.z = Thickness;
            llSetScale(Scale);
            return;
        }
        if (channel == 204007)
        {
            vector newColor = (vector)data;
            llSetColor(newColor, ALL_SIDES);
        }
    }
}
