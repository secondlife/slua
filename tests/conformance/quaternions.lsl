quaternion v = <1,2,3,4>;

default {
  state_entry() {
    print((string)<1, 2, 3, 4>);
    print((string)v);

    // Rotation math precision. Every component is a full-mantissa float so
    // every multiply rounds under single precision but is exact under double.
    // Results are in the thousands, where one float ULP is around 1e-3 and so
    // shows up in the five decimals of the string form.
    vector vec = <12345.678, -9876.543, 5432.1>;
    rotation r = <0.2345678, -0.5432109, 0.6789012, 0.4321098>;
    print((string)(vec * r));
    // Near 180 degrees: the s*s and x*x terms nearly cancel against y*y and z*z
    print((string)(<100000.01, -0.001, 12345.6> * <0.6, 0.4, 0.6, 0.3464102>));
    // Non-unit rotation, as produced by scripts that skip normalization
    print((string)(<-3456.7, 7654.3, -9876.5> * <2.3, -3.7, 4.1, 1.9>));

    print((string)(<1234.5678, -2345.6789, 3456.789, -4567.891> * <98.7654, -87.6543, 76.5432, -65.4321>));
    // rhs products are exact, so only the order of the additions can differ
    print((string)(<-9876.543, 123.4567, 8765.432, -0.2345678> * <0.5, -0.5, 0.5, 0.5>));
  }
}
