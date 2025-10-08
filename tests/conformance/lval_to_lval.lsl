float glob = 4.0;
default {
  state_entry() {
    float f = 2.0;
    float g = f;
    glob = g;
    g = glob;
    print(g);

    // make sure the result of an assignment can be captured
    print(g = glob = 4.0);
    print(glob);

    // Make sure things are captured as temporaries before they're replaced
    // if the left hand side of an expr will mutate.
    f = 4.0;
    print((f = 2.0) + f);
    print(f);

    // This shouldn't matter for globals (they're already loaded into a temporary,)
    // but let's be sure.
    glob = 4.0;
    print((glob = 2.0) + glob);
    print(glob);

    f = 4.0;
    // If we do LOP_MOVE elision for `++f` results this may break!
    print((f++) + (++f));

    f = 4.0;
    // Same as a bove
    print((++f) + (f++));
  }
}
