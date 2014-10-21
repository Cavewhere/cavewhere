/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWREADINGENUMS_H
#define CWREADINGENUMS_H

namespace cwDistanceStates {
enum State {
    Valid = 0, //Distance data should be valid
    Empty = 1 //User hasn't enter any data
};
}

namespace cwCompassStates {
enum State {
    Valid = 0, //Distance data should be valid
    Empty = 1 //User hasn't enter any data
};
}

namespace cwClinoStates {
enum State {
    Valid = 0, //Distance data should be valid
    Empty = 1, //User hasn't enter any data
    Down = 2, //User has enter the keyword down
    Up = 3 //User has enter the keyword up
};
}

namespace cwDepthStates {
enum State {
    Valid = 0, //Distance data should be valid
    Empty = 1, //User hasn't enter any data
};
}



#endif // CWREADINGENUMS_H
