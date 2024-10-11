/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import Qt 4.7

QQ.State {

    QQ.PropertyChanges {
        target: edittor
        visible: true
    }

    QQ.PropertyChanges {
        target: dataText
        visible: false
    }

    QQ.PropertyChanges {
        id: dataTextPropertyChanges
        target: dataTextInput

        QQ.Keys.onPressed: {
            NavigationHandler.HandleTabEvent(event, dataBox);
            NavigationHandler.HandleArrowEvent(event, dataBox);
            if(event.accepted) {
                dataBox.state = ''; //Default state
            }
        }

        QQ.Keys.onEnterPressed: {
            dataBox.state = '';
        }

        QQ.Keys.onReturnPressed: {
            dataBox.state = '';
        }

        onFocusChanged: {
            if(!focus) {
                dataBox.state = '';
            }
        }

    }

    QQ.PropertyChanges {
        target: dataBox
        z: 1
    }
}
