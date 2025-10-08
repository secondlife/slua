
default {
  state_entry(){
    list foo = [1, 2, 3];
    list bar = [2];

    print(foo != bar);
    print(bar != foo);
    print(foo != []);
    print(foo == bar);
    print(foo == foo);

    // Try some nested stuff to make sure we didn't mess up our reg allocs.
    print((bar + [([1] != [0])]) != []);
  }
}
