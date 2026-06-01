// Tests all the cases where `<int> MUL_ASSIGN <float>` won't cause an errors.
// Essentially, all the cases where it's not treated as an rvalue.
// The error case is in `mul_assign_error.lsl`.
integer g;

default {
  state_entry() {
    integer a;
    float b = 2.0;

    // Expression statement
    a = 3;
    a *= b;

    // Should work with globals as well
    g = 3;
    g *= b;

    // For loop increment
    a = 1;
    integer i;
    for (i = 0; i < 3; a *= b) {
      ++i;
    }

    // For loop init
    a = 5;
    for (a *= b; i < 6; ++i) {
    }

    // Should be the same even if it's in parens
    (a *= b);
  }
}
