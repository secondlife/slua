
default {
  state_entry(){
    float foo = 1.0;
    float bar = 2.0;
    float baz = foo + bar;
    string other = "foo";
    string new = other + "bar";
    print(foo + bar);
    print(new);
  }
}
