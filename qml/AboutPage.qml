import QtQuick 2.0 as QQ
import QtQuick.Layouts 1.12
import Cavewhere 1.0

StandardPage {

    ColumnLayout {
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
