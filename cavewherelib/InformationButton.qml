/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// import QtQuick 2.0 as QQ // to target S60 5th Edition or Maemo 5
import QtQuick 2.0 as QQ

IconButton {
    property QQ.Item showItemOnClick
    iconSource: "qrc:icons/Information.png"
    hoverIconSource: "qrc:icons/InformationDark.png"
    sourceSize: Qt.size(15, 15);
    height: 15
    width: 15

    onClicked:  {
        if(showItemOnClick !== null) {
            showItemOnClick.visible = !showItemOnClick.visible
        }
    }

}
