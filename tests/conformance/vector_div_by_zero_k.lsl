// Test that vector division by zero throws a runtime error
default {
    state_entry() {
        vector v = <1, 1, 1> / 0;  // This should error
        print((string)v);
    }
}
