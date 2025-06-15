import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QuickQanava as Qan
import cavewherelib

Qan.NodeItem {

    property FileNameArtifact artifact

    implicitWidth: layout.width + 10
    implicitHeight: rectId.implicitHeight

    Rectangle {
        id: rectId
        implicitWidth: layout.width + 10
        implicitHeight: layout.height + 10
        border.width: 1

        ColumnLayout {
            id: layout
            x: 5
            y: 5
            width: 150

            Text {
                text: "Temporary File:"
                font.bold: true
            }

            LinkText {
                id: textId
                Layout.fillWidth: true
                text: artifact.filename
                elide: Text.ElideMiddle
                onClicked: {
                    // Show the popup menu on click
                    menu.popup()
                }
            }
        }
    }

    // Popup menu with options to open file or show directory in Finder
    Menu {
        id: menu
        MenuItem {
            text: "Copy Path"
            onTriggered: {
                RootData.copyText(artifact.filename);
            }
        }

        MenuSeparator {}

        MenuItem {
            text: "Open File"
            onTriggered: {
                // Assuming cavewherelib provides openFile()
                Qt.openUrlExternally("file:///" + artifact.filename);
            }
        }
        MenuItem {
            text: {
                if (Qt.platform.os === "osx") {
                    return "Show in Finder";
                } else if (Qt.platform.os === "windows") {
                    return "Show in Explorer";
                } else {
                    return "Show in File Manager";
                }
            }

            onTriggered: {
                // Assuming cavewherelib provides showInFinder() to reveal the file's directory
                RootData.showInFolder(artifact.filename)
            }
        }
    }
}
