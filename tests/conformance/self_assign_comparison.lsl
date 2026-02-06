integer g = 0;

checkTruth(string name, integer val)
{
    if (!val)
    {
        llOwnerSay(name + ": FAILED");
        llOwnerSay((string)(0/0));
    }
}

testParam(integer arg)
{
    arg = arg != 0;
    checkTruth("param self-assign !=", arg == 0);
}

default {
    state_entry() {
        integer l = 0;
        l = l != 0;
        checkTruth("local self-assign !=", l == 0);

        g = g != 0;
        checkTruth("global self-assign !=", g == 0);

        testParam(0);

        integer m = 0;
        m = m == 0;
        checkTruth("local self-assign ==", m == 1);

        integer n = 5;
        n = n != 0;
        checkTruth("local self-assign != (nonzero)", n == 1);

        integer p = 0;
        p = p < 1;
        checkTruth("local self-assign <", p == 1);

        // RHS aliasing: target is same as rhs operand
        integer r = 0;
        r = 0 != r;
        checkTruth("local self-assign != (rhs)", r == 0);

        integer s = 0;
        s = 1 > s;
        checkTruth("local self-assign > (rhs)", s == 1);
    }
}
