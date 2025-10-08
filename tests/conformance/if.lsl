string truthTeller(integer someVal)
{
  if (someVal) return "yes";
  return "no";
}

default {
  state_entry() {
    print(truthTeller(1));
    print(truthTeller(0));
    print(truthTeller(-1));
  }
}
