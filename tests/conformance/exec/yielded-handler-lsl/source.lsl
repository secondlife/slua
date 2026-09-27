integer done = 0;

default {
    state_entry() {
        integer i = 0;
        while (i < 200) {
            i = i + 1;
        }
        done = i;
    }
    timer() {
    }
    moving_start() {
        if (done == 200)
            print("done ok");
        else
            print("done bad");
    }
}
