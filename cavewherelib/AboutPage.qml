import QtQuick.Layouts 1.12
import cavewherelib

StandardPage {

    ColumnLayout {
        id: columnId
        anchors.centerIn: parent

        spacing: 10

        CavewhereLogo {
            Layout.alignment: Qt.AlignHCenter
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "Version: " + RootData.version
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: {
                return "Copyright 2024 Philip Schuchardt"
            }
        }
    }
}
