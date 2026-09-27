integer counter = 5;

default {
    state_entry() {
        counter = counter + 1;
        state other;
    }
    state_exit() {
        counter = counter + 10;
    }
}

state other {
    state_entry() {
        counter = counter + 100;
    }
    moving_start() {
        if (counter == 116)
            print("counter ok");
        else
            print("counter bad");
    }
}
