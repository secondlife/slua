vector v = <1.0, 2.0, 3.0>;

default {
  state_entry(){
    // different precisions depending on whether or not
    // it's stored in a list!
    print((string)v);
    print((string)[v]);
  }
}
