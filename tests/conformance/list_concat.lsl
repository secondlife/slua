
default {
  state_entry() {
    list a = [1, 2, 3];
    list b = [4, 5, 6];
    print((string)(a + b));
    print((string)(a + "foobar"));
    print((string)("foobar" + a));
  }
}
