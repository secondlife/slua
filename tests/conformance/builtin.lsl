string foofunc()
{
  return "foo";
}

default {
  state_entry() {
    print(foofunc() + llGetSubString("foobar", 3, 5) + "baz");
  }
}
