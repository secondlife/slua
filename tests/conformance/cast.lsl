
default {
  state_entry(){
    float foo = (integer)"1";
    print((string)[(string)(foo + foo * 4), "hello!"]);
  }
}
