vector g_v = <1.0, 2.0, 3.0>;

default {
  state_entry() {
    vector v = <1.0, 2.0, 3.0>;
    print((string)(++v.x));
    print((string)(v.z++));
    print((string)v);

    print((string)(++g_v.x));
    print((string)(g_v.z++));
    print((string)g_v);
  }
}
