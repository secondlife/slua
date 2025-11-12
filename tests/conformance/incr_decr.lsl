float g_f = 1.0;
integer g_i = 1;

checkTruth(string name, integer val)
{
    if (!val)
    {
        llOwnerSay(name + ": FAILED");
        // Force a crash
        llOwnerSay((string)(0/0));
    }
}

default {
  state_entry(){
    // Local float tests
    float f = 1.0;
    checkTruth("local float pre-increment", ++f == 2.0);
    checkTruth("local float post-decrement", f-- == 2.0);
    checkTruth("local float after post-decrement", f == 1.0);
    ++f;
    f++;
    checkTruth("local float after statement increments", f == 3.0);

    // Global float tests
    checkTruth("global float pre-increment", ++g_f == 2.0);
    checkTruth("global float post-decrement", g_f-- == 2.0);
    checkTruth("global float after post-decrement", g_f == 1.0);
    ++g_f;
    g_f++;
    checkTruth("global float after statement increments", g_f == 3.0);

    // Local integer tests
    integer i = 1;
    checkTruth("local integer pre-increment", ++i == 2);
    checkTruth("local integer post-decrement", i-- == 2);
    checkTruth("local integer after post-decrement", i == 1);
    ++i;
    i++;
    checkTruth("local integer after statement increments", i == 3);

    // Global integer tests
    checkTruth("global integer pre-increment", ++g_i == 2);
    checkTruth("global integer post-decrement", g_i-- == 2);
    checkTruth("global integer after post-decrement", g_i == 1);
    ++g_i;
    g_i++;
    checkTruth("global integer after statement increments", g_i == 3);
  }
}
