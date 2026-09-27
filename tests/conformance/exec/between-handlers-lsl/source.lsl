integer counter = 5;

default {
    state_entry() {
        counter = counter + 1;
    }
    timer() {
        counter = counter + 1;
    }
    moving_start() {
        if (counter == 6)
            print("counter ok");
        else
            print("counter bad");
    }
}
