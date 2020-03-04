import QtQuick 2.0 as QQ
import Cavewhere 1.0

StandardPage {

    QQ.Column {
        id: columnId
        anchors.centerIn: parent

        spacing: 10

        CavewhereLogo {
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Version: " + version
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: {
                return "Copyright 2020 Philip Schuchardt"
            }
        }
    }

}
