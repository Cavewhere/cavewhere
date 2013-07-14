/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import Qt 4.7

State {

    PropertyChanges {
        target: edittor
        visible: true
    }

    PropertyChanges {
        target: dataText
        visible: false
    }

    PropertyChanges {
        id: dataTextPropertyChanges
        target: dataTextInput

        Keys.onPressed: {
            NavigationHandler.HandleTabEvent(event, dataBox);
            NavigationHandler.HandleArrowEvent(event, dataBox);
            if(event.accepted) {
                dataBox.state = ''; //Default state
            }
        }

        Keys.onEnterPressed: {
            dataBox.state = '';
        }

        Keys.onReturnPressed: {
            dataBox.state = '';
        }

        onFocusChanged: {
            if(!focus) {
                dataBox.state = '';
            }
        }

    }

    PropertyChanges {
        target: dataBox
        z: 1
    }
}
