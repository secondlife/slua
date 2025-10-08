default {
  state_entry() {
    vector v = <1.0, 2.0, 3.0>;
    print((string)(v.x += 4.0));
    print((string)v);

    rotation r = <1.0, 2.0, 3.0, 4.0>;
    print((string)(r.s += 4.0));
    print((string)r);
  }
}
