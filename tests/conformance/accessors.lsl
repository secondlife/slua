default {
  state_entry() {
    vector v = <1.0,2.0,3.0>;
    quaternion r = <1.0,2.0,3.0,4.0>;
    print((string)v.z);
    print((string)r.s);
  }
}
