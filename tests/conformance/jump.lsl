default {
  state_entry() {
    jump foo;
    print("oh no!");
    @foo;
    print("yay!");

    integer i = 0;
    @start;
    if (i > 1)
      jump done;
    ++i;
    jump start;
    @done;
    print((string)i);
  }
}
