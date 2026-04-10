// Test that vector division by zero throws a runtime error
default {
    state_entry() {
        float y = 0.0;
        vector v = <1, 1, 1> / y;  // This should error
        print((string)v);
    }
}
