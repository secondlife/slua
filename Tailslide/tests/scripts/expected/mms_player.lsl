list /*clips*/_3;
string /*total_time*/_16;
integer /*num_clips*/_a;
integer /*clip_playing*/_1;
integer /*clip_preloading*/_2;
integer /*preset_clips*/_f;
integer /*notecard_line*/_9;
integer /*disable_touch*/_6;
integer /*disable_text*/_5;
integer /*die_on_unlink*/_4;
/*say*/_13(string /*str*/_19)
{
    llSay(0, /*str*/_19);
}

string /*format_float*/_7(float /*num*/_1c, integer /*after_dec*/_1a, integer /*chop_dec*/_1b)
{
    string /*str*/_25 = "";
    list /*x*/_26 = llParseString2List((string)/*num*/_1c, ["."], []);
    /*str*/_25 += llList2String(/*x*/_26, 0);
    string /*decimal*/_24 = llList2String(/*x*/_26, 1);
    if ((integer)/*decimal*/_24 != 0 || !/*chop_dec*/_1b)
    {
        /*str*/_25 += ".";
        /*str*/_25 += llGetSubString(/*decimal*/_24, 0, /*after_dec*/_1a - 1);
    }
    return /*str*/_25;
}

/*set_text*/_15(string /*str*/_1e, vector /*color*/_1d)
{
    if (/*disable_text*/_5)
        return;
    llSetText(llGetObjectName() + "\n" + /*str*/_1e, /*color*/_1d, 1.00000);
}

integer /*check_control*/_0(integer /*num*/_1f)
{
    integer /*i*/_27;
    if (/*disable_touch*/_6)
        return FALSE;
    if (1)
        return TRUE;
    for (/*i*/_27 = 0; /*i*/_27 < /*num*/_1f; /*i*/_27++)
        if (llDetectedKey(/*i*/_27) == llGetOwner())
            return TRUE;
    return FALSE;
}

/*preload_next_clip*/_e(integer /*show_text*/_20)
{
    if (/*clip_preloading*/_2 < /*num_clips*/_a)
        llPreloadSound(llList2Key(/*clips*/_3, /*clip_preloading*/_2));
    if (/*show_text*/_20)
    {
        /*set_text*/_15("Preloading " + (string)(2 - /*clip_preloading*/_2) + " clip(s) " + "[" + /*format_float*/_7(4.50000 * (2 - /*clip_preloading*/_2), 1, 0) + " sec]\n" + "Click to start play immediately.", <0.00000, 0.00000, 1.00000>);
    }
    /*clip_preloading*/_2 += 1;
}

/*play_next_clip*/_b()
{
    llPlaySound(llList2Key(/*clips*/_3, /*clip_playing*/_1), 1.00000);
    /*clip_playing*/_1 += 1;
}

/*update_text*/_17()
{
    /*set_text*/_15("Playing: " + /*format_time*/_8((integer)llGetTime()) + "/" + /*total_time*/_16, <0.00000, 1.00000, 0.00000>);
}

string /*format_time*/_8(integer /*secs*/_21)
{
    return (string)((integer)(/*secs*/_21 / 60)) + ":" + llGetSubString("0" + (string)(/*secs*/_21 % 60), -2, -1);
}

/*send_message*/_14(integer /*msg*/_23, list /*data*/_22)
{
    llMessageLinked(LINK_SET, /*msg*/_23, llList2CSV(/*data*/_22), "MASA MUSIC SCRIPT");
}

default
{
    state_entry()
    {
        if (1)
            llSetTextureAnim(FALSE, ALL_SIDES, 0, 0, 0, 0, 0);
        /*clip_playing*/_1 = 0;
        /*clip_preloading*/_2 = 0;
        /*num_clips*/_a = llGetListLength(/*clips*/_3);
        if (/*num_clips*/_a > 0)
        {
            /*preset_clips*/_f = TRUE;
            /*total_time*/_16 = /*format_time*/_8((integer)(/*num_clips*/_a * 9.00000));
        }
        llStopSound();
        /*set_text*/_15("Stopped", <1.00000, 0.00000, 0.00000>);
        /*send_message*/_14(20100, []);
        if (llGetStartParameter() == 222646)
            /*die_on_unlink*/_4 = TRUE;
        else
            /*die_on_unlink*/_4 = FALSE;
    }

    on_rez(integer /*param*/_28)
    {
        state /*reset*/_12;
    }

    touch_start(integer /*num*/_29)
    {
        if (/*check_control*/_0(/*num*/_29))
        {
            if (/*preset_clips*/_f)
                state /*preload*/_d;
            else if (llGetInventoryKey("sounds") != NULL_KEY)
                state /*read_notecard*/_11;
            else if (llGetInventoryNumber(INVENTORY_SOUND) > 0)
                state /*read_inventory*/_10;
            else
                /*say*/_13("nothing to play!");
        }
    }

    link_message(integer /*sender*/_2d, integer /*msg*/_2c, string /*data*/_2a, key /*domain*/_2b)
    {
        if (/*domain*/_2b != "MASA MUSIC SCRIPT")
            return;
        if (/*msg*/_2c == 10000)
        {
            if (/*preset_clips*/_f)
                state /*preload*/_d;
            else if (llGetInventoryKey("sounds") != NULL_KEY)
                state /*read_notecard*/_11;
            else if (llGetInventoryNumber(INVENTORY_SOUND) > 0)
                state /*read_inventory*/_10;
            else
                /*say*/_13("nothing to play!");
        }
        else if (/*msg*/_2c == 11000)
        {
            /*disable_touch*/_6 = (integer)/*data*/_2a;
        }
        else if (/*msg*/_2c == 12000)
        {
            /*disable_text*/_5 = (integer)/*data*/_2a;
            if (/*disable_text*/_5)
                llSetText("", <0.00000, 0.00000, 0.00000>, 0);
        }
    }

    changed(integer /*what*/_2e)
    {
        if (/*what*/_2e & CHANGED_LINK && llGetLinkNumber() == 0 && /*die_on_unlink*/_4)
            llDie();
    }
}
state /*reset*/_12
{
    state_entry()
    {
        /*disable_touch*/_6 = 0;
        /*disable_text*/_5 = 0;
        state default;
    }
}
state /*read_notecard*/_11
{
    state_entry()
    {
        /*notecard_line*/_9 = 0;
        llGetNotecardLine("sounds", /*notecard_line*/_9++);
        /*send_message*/_14(10300, []);
        /*set_text*/_15("reading notecard", <0.00000, 0.00000, 1.00000>);
        llSetTimerEvent(5);
        /*clips*/_3 = [];
    }

    dataserver(key /*qid*/_30, string /*data*/_2f)
    {
        if (/*data*/_2f == EOF)
        {
            /*num_clips*/_a = llGetListLength(/*clips*/_3);
            if (/*num_clips*/_a <= 0)
            {
                /*say*/_13("no clips");
                state default;
            }
            else
            {
                /*total_time*/_16 = /*format_time*/_8((integer)(/*num_clips*/_a * 9.00000));
                state /*preload*/_d;
            }
        }
        /*clips*/_3 += llCSV2List(/*data*/_2f);
        llGetNotecardLine("sounds", /*notecard_line*/_9++);
        llResetTime();
    }

    timer()
    {
        if (llGetTime() > 5.00000)
        {
            /*say*/_13("dataserver timeout");
            state default;
        }
    }

    state_exit()
    {
        llSetTimerEvent(0);
    }

    changed(integer /*what*/_31)
    {
        if (/*what*/_31 & CHANGED_LINK && llGetLinkNumber() == 0 && /*die_on_unlink*/_4)
            llDie();
    }

    on_rez(integer /*param*/_32)
    {
        state /*reset*/_12;
    }

    link_message(integer /*sender*/_36, integer /*msg*/_35, string /*data*/_33, key /*domain*/_34)
    {
        if (/*domain*/_34 != "MASA MUSIC SCRIPT")
            return;
        if (/*msg*/_35 == 10100)
            state default;
        else if (/*msg*/_35 == 11000)
            /*disable_touch*/_6 = (integer)/*data*/_33;
        else if (/*msg*/_35 == 12000)
            /*disable_text*/_5 = (integer)/*data*/_33;
    }
}
state /*read_inventory*/_10
{
    state_entry()
    {
        integer /*i*/_37;
        /*send_message*/_14(10300, []);
        /*set_text*/_15("reading inventory", <1.00000, 0.00000, 0.00000>);
        /*num_clips*/_a = llGetInventoryNumber(INVENTORY_SOUND);
        /*total_time*/_16 = /*format_time*/_8((integer)(/*num_clips*/_a * 9.00000));
        /*clips*/_3 = [];
        for (/*i*/_37 = 0; /*i*/_37 < /*num_clips*/_a; /*i*/_37++)
            /*clips*/_3 += [llGetInventoryName(INVENTORY_SOUND, /*i*/_37)];
        state /*preload*/_d;
    }

    changed(integer /*what*/_38)
    {
        if (/*what*/_38 & CHANGED_LINK && llGetLinkNumber() == 0 && /*die_on_unlink*/_4)
            llDie();
    }

    on_rez(integer /*param*/_39)
    {
        state /*reset*/_12;
    }
}
state /*preload*/_d
{
    state_entry()
    {
        /*send_message*/_14(21000, [/*num_clips*/_a, 9.00000]);
        /*send_message*/_14(10200, []);
        /*preload_next_clip*/_e(TRUE);
        llSetTimerEvent(4.50000);
    }

    touch_start(integer /*num*/_3a)
    {
        if (/*check_control*/_0(/*num*/_3a))
            state /*playing*/_c;
    }

    timer()
    {
        if (/*clip_preloading*/_2 >= 2 || /*clip_preloading*/_2 >= /*num_clips*/_a)
            state /*playing*/_c;
        /*preload_next_clip*/_e(TRUE);
    }

    link_message(integer /*sender*/_3e, integer /*msg*/_3d, string /*data*/_3b, key /*domain*/_3c)
    {
        if (/*domain*/_3c != "MASA MUSIC SCRIPT")
            return;
        if (/*msg*/_3d == 10000)
            state /*playing*/_c;
        else if (/*msg*/_3d == 10100)
            state default;
        else if (/*msg*/_3d == 11000)
            /*disable_touch*/_6 = (integer)/*data*/_3b;
        else if (/*msg*/_3d == 12000)
            /*disable_text*/_5 = (integer)/*data*/_3b;
    }

    state_exit()
    {
        llSetTimerEvent(0);
    }

    changed(integer /*what*/_3f)
    {
        if (/*what*/_3f & CHANGED_LINK && llGetLinkNumber() == 0 && /*die_on_unlink*/_4)
            llDie();
    }

    on_rez(integer /*param*/_40)
    {
        state /*reset*/_12;
    }
}
state /*playing*/_c
{
    state_entry()
    {
        llSetSoundQueueing(TRUE);
        llSetTimerEvent(1);
        /*send_message*/_14(20000, []);
        llResetTime();
        if (1)
            llSetTextureAnim(35, ALL_SIDES, 0, 0, 0, TWO_PI, 30.0000);
        /*play_next_clip*/_b();
        /*preload_next_clip*/_e(FALSE);
        /*update_text*/_17();
        if (/*clip_playing*/_1 >= /*num_clips*/_a)
            state /*wind_down*/_18;
    }

    timer()
    {
        if ((integer)((llGetTime() + 1.00000) / 9.00000) >= /*clip_playing*/_1)
        {
            /*play_next_clip*/_b();
            /*preload_next_clip*/_e(FALSE);
            if (/*clip_playing*/_1 >= /*num_clips*/_a)
                state /*wind_down*/_18;
        }
        /*update_text*/_17();
    }

    touch_start(integer /*num*/_41)
    {
        if (/*check_control*/_0(/*num*/_41))
            state default;
    }

    link_message(integer /*sender*/_45, integer /*msg*/_44, string /*data*/_42, key /*domain*/_43)
    {
        if (/*domain*/_43 != "MASA MUSIC SCRIPT")
            return;
        if (/*msg*/_44 == 10100)
            state default;
        else if (/*msg*/_44 == 11000)
            /*disable_touch*/_6 = (integer)/*data*/_42;
        else if (/*msg*/_44 == 12000)
            /*disable_text*/_5 = (integer)/*data*/_42;
    }

    state_exit()
    {
        llSetTimerEvent(0);
    }

    changed(integer /*what*/_46)
    {
        if (/*what*/_46 & CHANGED_LINK && llGetLinkNumber() == 0 && /*die_on_unlink*/_4)
            llDie();
    }

    on_rez(integer /*param*/_47)
    {
        state /*reset*/_12;
    }
}
state /*wind_down*/_18
{
    state_entry()
    {
        llSetTimerEvent(1);
    }

    timer()
    {
        if (llGetTime() >= (/*num_clips*/_a * 9.00000))
            state default;
        /*update_text*/_17();
    }

    touch_start(integer /*num*/_48)
    {
        if (/*check_control*/_0(/*num*/_48))
            state default;
    }

    link_message(integer /*sender*/_4c, integer /*msg*/_4b, string /*data*/_49, key /*domain*/_4a)
    {
        if (/*domain*/_4a != "MASA MUSIC SCRIPT")
            return;
        if (/*msg*/_4b == 10100)
            state default;
        else if (/*msg*/_4b == 11000)
            /*disable_touch*/_6 = (integer)/*data*/_49;
        else if (/*msg*/_4b == 12000)
            /*disable_text*/_5 = (integer)/*data*/_49;
    }

    state_exit()
    {
        llSetTimerEvent(0);
    }

    changed(integer /*what*/_4d)
    {
        if (/*what*/_4d & CHANGED_LINK && llGetLinkNumber() == 0 && /*die_on_unlink*/_4)
            llDie();
    }

    on_rez(integer /*param*/_4e)
    {
        state /*reset*/_12;
    }
}
