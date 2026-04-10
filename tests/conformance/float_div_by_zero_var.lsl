// Test that float division by zero throws a runtime error
default {
    state_entry() {
        float a = 1.0;
        float b = 0.0;
        float c = a / b;  // This should error
        print((string)c);
    }
}
