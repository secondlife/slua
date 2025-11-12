float glob = 4.0;
vector gVec = <0, 0, 0>;

checkTruth(string name, integer val)
{
    if (!val)
    {
        llOwnerSay(name + ": FAILED");
        // Force a crash by printing NaN
        llOwnerSay((string)(0/0));
    }
}

default {
  state_entry() {
    float f = 2.0;
    float g = f;
    glob = g;
    g = glob;
    print(g);

    // make sure the result of an assignment can be captured
    print(g = glob = 4.0);
    print(glob);

    // Make sure things are captured as temporaries before they're replaced
    // if the left hand side of an expr will mutate.
    f = 4.0;
    print((f = 2.0) + f);
    print(f);

    // This shouldn't matter for globals (they're already loaded into a temporary,)
    // but let's be sure.
    glob = 4.0;
    print((glob = 2.0) + glob);
    print(glob);

    f = 4.0;
    // If we do LOP_MOVE elision for `++f` results this may break!
    print((f++) + (++f));

    f = 4.0;
    // Same as a bove
    print((++f) + (f++));

    // Vector member assignment tests - verify RHS evaluated only once
    integer counter1 = 0;
    gVec.x = ++counter1;
    checkTruth("global vec no double eval", counter1 == 1);
    checkTruth("global vec stores correct value", gVec.x == 1.0);

    integer counter2 = 5;
    vector lVec = <0, 0, 0>;
    lVec.y = counter2++;
    checkTruth("local vec no double eval", counter2 == 6);
    checkTruth("local vec stores correct value", lVec.y == 5.0);

    integer counter3 = 10;
    gVec.z = 2.0;
    float result = (gVec.z = ++counter3) + 5.0;
    checkTruth("assignment result in expr", result == 16.0);
    checkTruth("counter incremented once in expr", counter3 == 11);

    integer counter4 = 20;
    rotation r = <0, 0, 0, 1>;
    r.s = counter4++;
    checkTruth("quat no double eval", counter4 == 21);
    checkTruth("quat stores correct value", r.s == 20.0);
  }
}
