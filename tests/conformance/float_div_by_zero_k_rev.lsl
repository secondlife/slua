// Test that float division by zero throws a runtime error
default {
    state_entry() {
        float a = 0.0;
        float c = 1.0 / a;  // This should error
        print((string)c);
    }
}
