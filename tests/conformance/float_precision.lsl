// Value we expect if double precision was used for temporaries or storage
// This is the max integer value a float32 can store without rounding
// intentionally an integer!
integer F32_MAX = 16777216;
// Value we expect if the value was truncated to float32 space.
integer F32_TRUNCATED = 16777215;
float DELTA = 5;
float g_f;

checkTruth(string name, integer val)
{
    if (!val)
    {
        llOwnerSay(name + ": FAILED");
        // Force a crash.
        llOwnerSay((string)(0/0));
    }
}

float returnFloat(float val)
{
    // Takes in an arg in float64 range and tries to reduce it to float32
    return val - DELTA;
}

float returnDouble()
{
    // Returns something in float64 range.
    return F32_MAX + DELTA;
}

float sleepy()
{
    // Force a yield injection so UThreadInjector has to serialize the stack.
    llSleep(0.05);
    return 0.0;
}

default
{
    state_entry()
    {
        float l_f;
        vector l_v;
        checkTruth("simple", (F32_MAX + DELTA - DELTA) == F32_MAX);
        checkTruth("simple yield", ((sleepy() + F32_MAX + DELTA + sleepy()) - DELTA) == F32_MAX);

        checkTruth("float constant", (16777221.0 - DELTA) == F32_TRUNCATED);

        checkTruth("arg passthrough", (returnFloat(F32_MAX + DELTA)) == F32_TRUNCATED);
        checkTruth("return passthrough", (returnDouble() - DELTA) == F32_TRUNCATED);

        checkTruth("loc lval passthrough", ((l_f = (F32_MAX + DELTA)) - DELTA) == F32_MAX);
        checkTruth("loc lval stored", (l_f - DELTA) == F32_TRUNCATED);

        float l_f_decl = F32_MAX + DELTA;
        checkTruth("loc lval declaration stored", (l_f_decl - DELTA) == F32_TRUNCATED);

        checkTruth("glob lval passthrough", ((g_f = (F32_MAX + DELTA)) - DELTA) == F32_TRUNCATED);
        checkTruth("glob lval stored", (g_f - DELTA) == F32_TRUNCATED);

        checkTruth("vec passthrough", ((l_v.x = (F32_MAX + DELTA)) - DELTA) == F32_TRUNCATED);
        checkTruth("vec stored", (l_v.x - DELTA) == F32_TRUNCATED);

        // If float64 was used these should round-trip.
        l_f = F32_MAX;
        checkTruth("loc lval preincr", (++l_f - 1) == F32_MAX);
        // it's as if the incr never happened here.
        checkTruth("loc lval preincr stored", l_f == F32_MAX);

        g_f = F32_MAX;
        // is basically F32 - 1 even though there was a preincr
        checkTruth("glob lval preincr", (++g_f - 1) == F32_TRUNCATED);
        // as if the incr never happened
        checkTruth("glob lval preincr stored", g_f == F32_MAX);

        l_v.x = F32_MAX;
        // is basically F32 - 1 even though there was a preincr
        checkTruth("vec preincr", (++l_v.x - 1) == F32_TRUNCATED);
        // as if the incr never happened
        checkTruth("vec preincr stored", l_v.x == F32_MAX);

        // These are all roundtrip fine because the temporaries are still float64.
        l_f = F32_MAX;
        checkTruth("loc lval postincr", (l_f++ + 1 - 1) == F32_MAX);
        // it's as if the incr never happened here.
        checkTruth("loc lval postincr stored", l_f == F32_MAX);

        g_f = F32_MAX;
        checkTruth("glob lval postincr", (g_f++ + 1 - 1) == F32_MAX);
        // as if the incr never happened
        checkTruth("glob lval postincr stored", g_f == F32_MAX);

        l_v.x = F32_MAX;
        checkTruth("vec postincr", (l_v.x++ + 1 - 1) == F32_MAX);
        // as if the incr never happened
        checkTruth("vec postincr stored", l_v.x == F32_MAX);

        // This should roundtrip correctly, integer->float returns a double.
        // By the time we cast back to `integer` it should be back in float32 range, so it
        // should roundtrip correctly.
        checkTruth("integer->float cast", (integer)((float)(F32_MAX + 5) - 5) == F32_MAX);

        // This is _not_ meant to roundtrip correctly. int->float returns a double but float->int takes a float32.
        checkTruth("integer->float->integer cast", ((integer)((float)0x7FffFFfe)) == -2147483648);

        // This is _also_ not meant to roundtrip correctly. Mono has very janky precision when using the
        // F<n> format specifier, we need to replicate it. We've ported some of the Mono NumberFormatter
        // code to C++ for this purpose.
        l_f = F32_MAX;
        checkTruth("float->string cast", ((integer)((string)l_f)) == F32_MAX + 4);
        // string->float is fine though
        checkTruth("string->float cast", (float)((string)F32_MAX) == l_f);
    }
}
