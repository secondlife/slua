float g_f = 1.0;

default {
  state_entry(){
    float f = 1.0;
    print(++f);
    print(f--);
    print(f);
    ++f;
    f++;
    print(f);

    // now globals
    print(++g_f);
    print(g_f--);
    print(g_f);
    ++g_f;
    g_f++;
    print(g_f);
  }
}
