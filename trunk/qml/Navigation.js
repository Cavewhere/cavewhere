// tab.js Handles tabing between text elements
.pragma library

/**
Based on the key. This function will iether tab forward or backward
based on the tab object.
*/
function handleTabEvent(event, object) { //nextTabObject, previousTabObject) {
    // console.log('Key pressed ' + event.key + " " + event.text);

    var nextTabObject = object.nextTabObject;
    var previousTabObject = object.previousTabObject;

    if(event.key === Qt.Key_Tab) {
       // console.log('Tab pressed');
        if(nextTabObject !== null) {
            nextTabObject.focus = true;
            event.accepted = true;
        }
    } else if(event.key === 1 + Qt.Key_Tab) {
        //Shift tab -- 1 + Qt.Key_Tab is a hack but it works
       // console.log('Shift Tab pressed');
        if(previousTabObject !== null) {
            previousTabObject.focus = true;
            event.accepted = true;
        }
    }
}

/**
\brief Handles arrow keys
*/
function handleArrowEvent(event, object) {

    if(event.key === Qt.Key_Left) {
        if(object.navLeftObject) {
            object.navLeftObject.focus = true;
            event.accepted = true
        }
    } else if(event.key === Qt.Key_Right) {
        if(object.navRightObject) {
            object.navRightObject.focus = true;
            event.accepted = true
        }
    } else if(event.key === Qt.Key_Up) {
        if(object.navUpObject) {
            object.navUpObject.focus = true;
            event.accepted = true
        }
    } else if(event.key === Qt.Key_Down) {
        if(object.navDownObject) {
            object.navDownObject.focus = true;
            event.accepted = true
        }
    }

}

/**
\brief handle enter navigation
*/
function enterNavigation(event, object) {
    if(event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
        object.nextTabObject.focus = true;
        event.accepted = true;
    }
}
