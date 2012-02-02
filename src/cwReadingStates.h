#ifndef CWREADINGENUMS_H
#define CWREADINGENUMS_H

namespace cwDistanceStates {
enum State {
    Valid, //Distance data should be valid
    Empty //User hasn't enter any data
};
}

namespace cwCompassStates {
enum State {
    Valid, //Distance data should be valid
    Empty //User hasn't enter any data
};
}

namespace cwClinoStates {
enum State {
    Valid, //Distance data should be valid
    Empty, //User hasn't enter any data
    Down, //User has enter the keyword down
    Up //User has enter the keyword up
};
}



#endif // CWREADINGENUMS_H
