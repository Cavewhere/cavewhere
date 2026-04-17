import QtQuick.Layouts
import QtQuick.Controls as QC
import QtQuick as QQ
import cavewherelib

StandardPage {

    ColumnLayout {
        id: columnId
        anchors.centerIn: parent

        spacing: 10

        CavewhereLogo {
            Layout.alignment: Qt.AlignHCenter
        }

        QC.Label {
            Layout.alignment: Qt.AlignHCenter
            text: "Version: " + RootData.version
        }

        QC.Label {
            Layout.alignment: Qt.AlignHCenter
            text: {
                return "Copyright 2026 Philip Schuchardt"
            }
        }
    }
}
