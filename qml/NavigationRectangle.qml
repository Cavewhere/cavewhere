import Qt 4.7


Item {
    id: tabRectangle;

    property variant nextTabObject
    property variant previousTabObject

    property variant navDownObject
    property variant navUpObject
    property variant navLeftObject
    property variant navRightObject

    signal focused

    onFocusChanged: {
        if(focus) {
            focused();
        }
    }

}
