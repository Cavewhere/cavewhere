/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// import QtQuick as QQ // to target S60 5th Edition or Maemo 5
import QtQuick as QQ

IconButton {
    property QQ.Item showItemOnClick
    iconSource: "qrc:/twbs-icons/icons/question-circle.svg"
    hoverIconSource: "qrc:/twbs-icons/icons/question-circle-fill.svg"
    sourceSize: Qt.size(15, 15);
    height: 15
    width: 15

    onClicked:  {
        if(showItemOnClick !== null) {
            showItemOnClick.visible = !showItemOnClick.visible
        }
    }

}
