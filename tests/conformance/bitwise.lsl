default {
  state_entry() {
    print(0x4 >> 1);
    print(0xFAaaAAaa >> 4);
    print(0xF << 4);
    print(3 & 2);
    print(1 | 2);
    print(3 ^ 5);
    print(~0xFFffFFf0);
  }
}
