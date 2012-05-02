// import QtQuick 1.0 // to target S60 5th Edition or Maemo 5
import QtQuick 2.0

Rectangle {
    property alias text: textId.text

    width: 150
    height: 62

    color: "red"

    Text {
        id: textId
    }

    MouseArea {


    }
}

