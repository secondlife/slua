
default {
  state_entry(){
    integer zero = 0;
    integer one = 1;
    integer two = 2;

    print(zero && zero);
    print(zero || zero);
    print(zero && one);
    print(zero || one);
    print(one && two);
    print(one || two);
  }
}
